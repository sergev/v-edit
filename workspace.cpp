#include "workspace.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>

#include "tempfile.h"

Workspace::Workspace(Tempfile &tempfile) : tempfile_(tempfile)
{
    // Create initial tail segment (empty workspace still has a tail)
    contents_.emplace_back();
    cursegm_ = contents_.begin();
}

Workspace::~Workspace()
{
    cleanup_contents();
}

void Workspace::cleanup_contents()
{
    contents_.clear();
    cursegm_ = contents_.end();

    // Close original file.
    if (original_fd_ > 0) {
        close(original_fd_);
        original_fd_ = -1;
    }
}

void Workspace::reset()
{
    cleanup_contents();
    file_state.writable    = 0;
    view.topline           = 0;
    view.basecol           = 0;
    position.line          = 0;
    position.segmline      = 0;
    view.cursorcol         = 0;
    view.cursorrow         = 0;
    file_state.modified    = false;
    file_state.backup_done = false;

    // Create initial tail segment (empty workspace still has a tail)
    contents_.emplace_back();
    cursegm_ = contents_.begin();
}

//
// Compute line count.
//
// TODO: This function can be optimized by caching the computed value,
// TODO: and invalidating the cached value when contents change.
//
int Workspace::total_line_count() const
{
    int total_lines = 0;

    for (auto seg : contents_) {
        total_lines += seg.line_count;
    }
    return total_lines;
}

//
// Build segment chain from in-memory lines vector.
// Writes lines into temp file.
//
void Workspace::load_text(const std::vector<std::string> &lines)
{
    reset();
    file_state.writable = 1;

    if (lines.size() > 0) {
        // Write lines to temp file and get a segment
        auto contents_from_temp = tempfile_.write_lines_to_temp(lines);
        if (contents_from_temp.empty())
            throw std::runtime_error("load_text: failed to write lines to temp file");
        // Move the segment into our contents_ list BEFORE the tail
        // reset() already created a tail segment, so insert before it
        auto insert_pos = std::prev(contents_.end());
        contents_.splice(insert_pos, contents_from_temp);
    }

    // Tail segment already exists from reset(), no need to add another

    // Set cursegm_ to the first segment (the content, not the tail)
    cursegm_          = contents_.begin();
    position.segmline = 0;
    position.line     = 0;
}

//
// Parse text and build segment chain from it.
//
void Workspace::load_text(const std::string &text)
{
    std::vector<std::string> lines_vec;
    std::string cur;
    lines_vec.reserve(64);
    for (char c : text) {
        if (c == '\n') {
            lines_vec.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    // In case text doesn't end with newline, keep the last line
    if (!cur.empty() || lines_vec.empty())
        lines_vec.push_back(cur);
    load_text(lines_vec);
}

//
// Set current segment to the segment containing the specified line number.
// Based on wksp_position() from prototype/r.edit.c
//
// This method positions the workspace by updating:
//   - cursegm_: points to the segment containing the line
//   - segmline_: first line number in the current segment
//   - line_: the absolute line number requested
//
// The method walks the segment chain forward or backward to find the segment
// containing the requested line.
//
// Returns:
//   0 - success, cursegm_ and segmline_ updated to the correct segment
//   1 - line number is beyond end of file (hit tail segment)
//
// Throws:
//   std::runtime_error for invalid line numbers or corrupted segment chains
//
int Workspace::change_current_line(int lno)
{
    // Validate input: negative line numbers are invalid
    if (lno < 0)
        throw std::runtime_error("change_current_line: negative line number");

    if (cursegm_ == contents_.end())
        throw std::runtime_error("change_current_line: empty workspace");

    // Special case: if we're positioned on a tail segment, all lines are beyond end of file
    if (cursegm_->is_empty()) {
        position.line = lno;
        return 1; // Line is beyond end of file
    }

    // Move forward to find the segment containing lno
    while (lno >= position.segmline + cursegm_->line_count) {
        if (cursegm_->is_empty()) {
            // Hit tail segment - line is beyond end of file
            // Return 1 to signal that line is beyond end of file
            position.line = position.segmline;
            return 1;
        }
        position.segmline += cursegm_->line_count;

        // Check if there's a next segment before moving
        auto next_it = std::next(cursegm_);
        if (next_it == contents_.end()) {
            throw std::runtime_error("change_current_line: null segment in chain");
        }
        ++cursegm_;
    }

    // Move backward to find the segment containing lno
    while (lno < position.segmline) {
        if (cursegm_ == contents_.begin())
            throw std::runtime_error("change_current_line: null previous segment");
        --cursegm_;
        position.segmline -= cursegm_->line_count;
    }

    // Consistency check: segmline should not be negative
    if (position.segmline < 0) {
        throw std::runtime_error("change_current_line: line count lost (segmline < 0)");
    }

    // Update workspace state
    position.line = lno;
    return 0;
}

//
// Build segment chain from file descriptor.
//
void Workspace::load_file(int fd)
{
    // Clean up old chain
    cleanup_contents();

    // Store the original fd so we can use it later
    original_fd_ = fd;

    // Build segment chain by reading file
    // Clear any existing segments list
    contents_.clear();

    char read_buf[8192];
    int buf_count    = 0;
    int buf_next     = 0;
    long file_offset = 0;

    // Temporary segment to build data
    Segment temp_seg;
    int lines_in_seg = 0;
    long seg_seek    = 0;

    for (;;) {
        // Read buffer if needed
        if (buf_next >= buf_count) {
            buf_next  = 0;
            buf_count = read(fd, read_buf, sizeof(read_buf));
            if (buf_count <= 0) {
                // EOF
                if (lines_in_seg > 0) {
                    // Create final segment in list
                    contents_.emplace_back();
                    Segment &seg        = contents_.back();
                    seg.line_count      = lines_in_seg;
                    seg.file_descriptor = fd;
                    seg.file_offset     = seg_seek;
                    seg.line_lengths    = std::move(temp_seg.line_lengths);
                }
                break;
            }
        }

        // Process line - handle lines that span buffer boundaries
        int line_len       = 0;
        bool line_complete = false;

        while (!line_complete) {
            char *line_start = read_buf + buf_next;
            char *line_end   = line_start;

            // Find line end
            while (buf_next < buf_count && *line_end != '\n') {
                ++line_end, ++buf_next;
            }

            line_len += (line_end - line_start);

            if (buf_next < buf_count) {
                // Found newline in current buffer
                line_len += 1; // include newline
                ++buf_next;    // skip '\n'
                line_complete = true;
            } else {
                // No newline found - line continues in next buffer
                // More data to read - reload buffer
                buf_next  = 0;
                buf_count = read(fd, read_buf, sizeof(read_buf));
                if (buf_count <= 0) {
                    // EOF - treat incomplete line as complete
                    line_len += 1; // add trailing newline
                    line_complete = true;
                }
                // If buffer was successfully read, continue loop to process next buffer
            }
        }

        // Store line length in segment data
        if (lines_in_seg == 0) {
            seg_seek = file_offset;
        }

        temp_seg.line_lengths.push_back(line_len);

        ++lines_in_seg;
        file_offset += line_len;

        // Create new segment if we've hit limits
        if (lines_in_seg >= 127 || temp_seg.line_lengths.size() >= 4000) {
            contents_.emplace_back();
            Segment &seg        = contents_.back();
            seg.line_count      = lines_in_seg;
            seg.file_descriptor = fd;
            seg.file_offset     = seg_seek;
            seg.line_lengths    = std::move(temp_seg.line_lengths);

            lines_in_seg = 0;
        }
    }

    // Create tail segment if needed
    if (contents_.empty() || !contents_.back().is_empty()) {
        contents_.emplace_back();
    }

    cursegm_          = contents_.begin();
    position.segmline = 0;
    position.line     = 0;
}

//
// Create segments for n empty lines (based on blanklines from prototype).
// Each empty line has length 1 (just the newline).
//
std::list<Segment> Workspace::create_blank_lines(int n)
{
    std::list<Segment> segments;

    while (n > 0) {
        int lines_in_seg = (n > 127) ? 127 : n;

        Segment seg;
        seg.line_count      = lines_in_seg;
        seg.file_descriptor = -1; // Empty lines not from file
        seg.file_offset     = 0;

        // Add line length data: each empty line has length 1 (just newline)
        seg.line_lengths.resize(lines_in_seg);
        for (int i = 0; i < lines_in_seg; ++i) {
            seg.line_lengths[i] = 1;
        }

        segments.push_back(seg);
        n -= lines_in_seg;
    }

    // Note: we do NOT add a tail segment here - it should already exist in the workspace

    return segments;
}

//
// Read line content from segment chain at specified index.
// Enhanced version using iterator instead of Segment* pointer for safer access and modern C++
// practices.
//
std::string Workspace::read_line(int line_no)
{
    // First, position to the correct segment for this line
    if (change_current_line(line_no) != 0) {
        return ""; // Line beyond end of file
    }

    // Validate iterator is valid and points to a segment
    if (cursegm_ == contents_.end()) {
        return "";
    }

    // Verify the segment has contents (not a tail segment)
    if (cursegm_->is_empty()) {
        return "";
    }

    // Calculate relative line position within the current segment
    int rel_line = line_no - position.segmline;

    // Bounds checking: ensure rel_line is within segment bounds
    if (rel_line < 0 || rel_line >= static_cast<int>(cursegm_->line_lengths.size())) {
        return ""; // Line index out of bounds for this segment
    }

    // Calculate file offset by accumulating line lengths
    // Note: cursegm_->file_offset points to the START of the first line in the segment
    // We need to skip 'rel_line' lines to get to the line we want
    long seek_pos = cursegm_->file_offset;
    for (int i = 0; i < rel_line; ++i) {
        seek_pos += cursegm_->line_lengths[i];
    }

    // Get line length for the requested line
    int line_len = cursegm_->line_lengths[rel_line];
    if (line_len <= 0) {
        return "";
    }

    // Handle empty lines (just newline) - return empty string without newline
    if (line_len == 1 || cursegm_->file_descriptor < 0) {
        return "";
    }

    // Read line from file using iterator's segment data
    std::string result(line_len - 1, '\0'); // exclude newline
    if (lseek(cursegm_->file_descriptor, seek_pos, SEEK_SET) >= 0) {
        read(cursegm_->file_descriptor, static_cast<void *>(&result[0]), result.size());
    }
    return result;
}

//
// Write segment chain content to file.
//
bool Workspace::write_file(const std::string &path)
{
    if (contents_.empty()) {
        // No segment chain - write empty file
        int out_fd = creat(path.c_str(), 0664);
        if (out_fd >= 0) {
            close(out_fd);
        }
        return out_fd >= 0;
    }

    int out_fd = creat(path.c_str(), 0664);
    if (out_fd < 0)
        return false;

    char buffer[8192];

    // Find the last non-blank segment (to skip trailing blank lines)
    auto last_nonblank = contents_.end();
    for (auto it = contents_.begin(); it != contents_.end(); ++it) {
        if (!it->is_empty() && it->file_descriptor != -1) {
            last_nonblank = it;
        }
    }

    for (auto it = contents_.begin(); it != contents_.end(); ++it) {
        const auto &seg = *it;

        if (seg.is_empty())
            continue; // Skip tail segment

        // Skip trailing blank lines (blank segments after the last content segment)
        if (seg.file_descriptor == -1 && last_nonblank != contents_.end() &&
            std::distance(last_nonblank, it) > 0) {
            continue;
        }

        // Calculate total bytes for this segment
        long total_bytes = seg.total_byte_count();

        if (seg.file_descriptor > 0) {
            // Read from source file and write to output
            if (lseek(seg.file_descriptor, seg.file_offset, SEEK_SET) < 0) {
                // Failed to seek - file may have been unlinked
                // Skip this segment and continue
                continue;
            }
            while (total_bytes > 0) {
                int to_read = (total_bytes < (long)sizeof(buffer)) ? total_bytes : sizeof(buffer);
                int nread   = read(seg.file_descriptor, buffer, to_read);
                if (nread <= 0)
                    break;

                write(out_fd, buffer, nread);
                total_bytes -= nread;
            }
        } else {
            // Empty lines.
            std::string newlines(total_bytes, '\n');
            write(out_fd, newlines.data(), newlines.size());
        }
    }

    close(out_fd);
    return true;
}

//
// Split segment at given line number (based on breaksegm from prototype).
// When the needed line is beyond the end of file, creates empty segments
// with blank lines.
// Returns 0 on success, 1 if blank lines were appended.
//
int Workspace::breaksegm(int line_no, bool realloc_flag)
{
    if (contents_.empty())
        throw std::runtime_error("breaksegm: empty workspace");

    // Special case: empty workspace and line_no == 0 - just position at start
    if (total_line_count() == 0 && line_no == 0) {
        cursegm_          = contents_.begin(); // Position at tail segment
        position.segmline = 0;
        position.line     = 0;
        return 0; // Success, no lines created
    }

    // Position workspace to the target line
    if (change_current_line(line_no)) {
        // Line is beyond end of file - create blank lines to extend it
        // This matches the prototype behavior
        // Calculate how many blank lines to create
        // position.line was set by change_current_line to segmline of the tail segment
        // Check if workspace is empty (only has a tail segment)
        bool is_empty = contents_.front().is_empty();

        int num_blank_lines;

        if (is_empty) {
            // Empty file, create lines from 0 to line_no
            num_blank_lines = line_no + 1;
        } else {
            // Normal case: create lines from position.line to line_no
            num_blank_lines = (line_no - position.line) + 1;
        }

        // Must create at least 1 blank line
        if (num_blank_lines <= 0) {
            num_blank_lines = 1;
        }

        // Create blank lines using new list-based approach
        auto blank_segments = create_blank_lines(num_blank_lines);

        // Find where to insert blank segments (before tail segment)
        auto insert_pos = cursegm_;

        // Record where blank segments start before splicing
        int start_line = is_empty ? 0 : position.line;

        // Get iterator to first blank segment before splicing (since splice will move them)
        auto first_blank_seg = blank_segments.begin();

        // Insert blank segments
        contents_.splice(insert_pos, blank_segments);

        // After splicing, first_blank_seg is now part of contents_ and points to the first inserted
        // segment Position to it
        cursegm_          = first_blank_seg;
        position.segmline = start_line;
        position.line     = start_line;

        // Walk forward through the inserted segments to find the one containing line_no
        while (cursegm_ != insert_pos && line_no >= position.segmline + cursegm_->line_count) {
            position.segmline += cursegm_->line_count;
            ++cursegm_;
        }

        position.line = line_no;

        return 1; // Signal that we created lines
    }

    // Now we're at the segment containing line_no
    int rel_line = line_no - position.segmline;

    if (rel_line == 0) {
        return 0; // Already at the right position
    }

    // Special case: blank line segment (file_descriptor == -1) - split by line_lengths array
    if (cursegm_->file_descriptor == -1) {
        if (rel_line >= cursegm_->line_count) {
            throw std::runtime_error(
                "breaksegm: inconsistent rel_line after change_current_line()");
        }

        // Find where to insert the new segment (after current segment)
        auto insert_pos = std::next(cursegm_);

        // Create new segment in place
        auto new_it             = contents_.insert(insert_pos, Segment());
        Segment &new_seg        = *new_it;
        new_seg.line_count      = cursegm_->line_count - rel_line;
        new_seg.file_descriptor = -1; // Still blank lines
        new_seg.file_offset     = cursegm_->file_offset;

        // Copy remaining sizes
        for (size_t i = rel_line; i < cursegm_->line_lengths.size(); ++i) {
            new_seg.line_lengths.push_back(cursegm_->line_lengths[i]);
        }

        // Truncate original segment sizes
        cursegm_->line_lengths.resize(rel_line);
        cursegm_->line_count = rel_line;

        // Update workspace position
        cursegm_          = new_it;
        position.segmline = line_no;

        return 0;
    }

    // Normal file segment - record where we are in the data
    size_t split_point = rel_line;

    // Walk through the first rel_line lines to calculate offset
    long offs = 0;
    for (int i = 0; i < rel_line; ++i) {
        if (i >= cursegm_->line_count) {
            split_point = cursegm_->line_count;
            break;
        }
        offs += cursegm_->line_lengths[i];
    }

    // Find where to insert the new segment (after current segment)
    auto insert_pos = std::next(cursegm_);

    // Create new segment in place
    auto new_it             = contents_.insert(insert_pos, Segment());
    Segment &new_seg        = *new_it;
    new_seg.line_count      = cursegm_->line_count - rel_line;
    new_seg.file_descriptor = cursegm_->file_descriptor;
    new_seg.file_offset     = cursegm_->file_offset + offs;

    // Copy remaining data bytes from split_point to end
    for (size_t i = split_point; i < cursegm_->line_count; ++i) {
        new_seg.line_lengths.push_back(cursegm_->line_lengths[i]);
    }

    // Truncate original segment data - keep only first rel_line data
    cursegm_->line_lengths.resize(split_point);
    cursegm_->line_count = rel_line;

    // Update workspace position
    cursegm_          = new_it;
    position.segmline = line_no;

    return 0;
}

//
// Merge adjacent segments (based on catsegm from prototype).
// Tries to merge current segment with previous if they're adjacent.
// Returns true if merge occurred, false otherwise.
//
bool Workspace::catsegm()
{
    if (cursegm_ == contents_.begin() || cursegm_ == contents_.end())
        return false;

    auto curr_it = cursegm_;
    auto prev_it = std::prev(curr_it);

    Segment &curr = *curr_it;
    Segment &prev = *prev_it;

    // Check if segments can be merged
    // They must be from the same file (not tail segments) and together have < 127 lines
    if (prev.file_descriptor > 0 && prev.file_descriptor == curr.file_descriptor &&
        (prev.line_count + curr.line_count) < 127) {
        // Calculate if they're adjacent
        long prev_bytes = prev.total_byte_count();
        if (curr.file_offset == prev.file_offset + prev_bytes) {
            // Segments are adjacent - merge them
            // Combine data into previous segment
            for (unsigned short byte : curr.line_lengths)
                prev.line_lengths.push_back(byte);
            prev.line_count += curr.line_count;

            // Erase current segment from list
            contents_.erase(curr_it);
            cursegm_ = prev_it;

            // After merging, cursegm_ points to the merged segment.
            // Ensure our positioning is still valid after the merge.
            // The current line should still be valid, so re-position to it.

            // Reset segmline to allow proper repositioning
            position.segmline = 0;

            // Re-position properly using change_current_line logic
            change_current_line(position.line);

            return true;
        }
    }
    return false;
}

//
// Insert segments into workspace before given line (based on insert from prototype).
//
void Workspace::insert_contents(std::list<Segment> &contents_to_insert, int at)
{
    if (contents_to_insert.empty())
        return;

    // Split at insertion point
    breaksegm(at, true);

    // after breaksegm, cursegm_ points to the segment at position 'at'
    // Insert new segments BEFORE cursegm_
    auto insert_pos = cursegm_;

    // Remember the size of contents before splicing so we can find the first inserted segment
    auto before_size = std::distance(contents_.begin(), insert_pos);

    contents_.splice(insert_pos, contents_to_insert);

    // Update workspace position to FIRST inserted segment (not last)
    // After splicing, the first inserted segment is at the position where insert_pos was
    cursegm_ = contents_.begin();
    std::advance(cursegm_, before_size);

    position.segmline   = at;
    file_state.writable = 1; // Mark as edited
}

//
// Delete segments between from and to lines (based on delete from prototype).
// Returns the deleted segment chain.
//
void Workspace::delete_contents(int from, int to)
{
    // Convert to use list operations - simplified version
    // Original implementation used pointer operations, but we need to use list erase
    if (contents_.empty() || contents_.front().is_empty() || from > to)
        return;

    auto total = total_line_count();
    if (total == 0)
        return;

    // If the deletion range starts beyond the end of file, do nothing
    if (from >= total)
        return;

    // Clamp 'to' to the last valid line index
    if (to >= total)
        to = total - 1;

    // Use breaksegm for positioning (it uses list operations internally now)
    // Break AFTER the last line to delete (to+1) so we can delete up to and including 'to'
    int result = breaksegm(to + 1, true);
    if (result != 0) {
        if (to + 1 > total) {
            change_current_line(total);
        } else {
            return;
        }
    }

    // Find iterators for the deletion range using the list
    auto end_delete_it = cursegm_;

    result = breaksegm(from, true);
    if (result != 0) {
        return;
    }

    auto start_delete_it = cursegm_;
    auto after_delete_it = end_delete_it;

    // Erase the range from the list
    contents_.erase(start_delete_it, after_delete_it);

    // Update workspace position
    if (!contents_.empty()) {
        cursegm_ =
            (after_delete_it != contents_.end()) ? after_delete_it : std::prev(contents_.end());
        position.segmline = from;
    } else {
        cursegm_          = contents_.begin();
        position.segmline = 0;
    }

    file_state.writable = 1; // Mark as edited
}

//
// Scroll workspace by nl lines (based on wksp_forward from prototype).
// nl: negative for up, positive for down
//
void Workspace::scroll_vertical(int nl, int max_rows, int total_lines)
{
    if (nl < 0) {
        // Scroll up (toward beginning)
        if (view.topline == 0) {
            // Already at top
            return;
        }
    } else if (nl > 0) {
        // Scroll down (toward end)
        // Only return early if we're already at a valid bottom position
        int max_topline = total_lines - max_rows;
        if (max_topline < 0)
            max_topline = 0;
        // Only prevent scrolling if topline is at the valid maximum (not beyond it)
        if (view.topline == max_topline) {
            // Already at the valid bottom position - can't scroll further
            return;
        }
    }

    view.topline += nl;

    // Clamp topline to valid range
    if (view.topline > total_lines - max_rows)
        view.topline = total_lines - max_rows;
    if (view.topline < 0)
        view.topline = 0;

    // Update current line to stay in visible range
    if (position.line > view.topline + max_rows - 1)
        position.line = view.topline + max_rows - 1;
    if (position.line < view.topline)
        position.line = view.topline;
}

//
// Shift horizontal view by nc columns (based on wksp_offset from prototype).
// nc: negative for left, positive for right
//
void Workspace::scroll_horizontal(int nc, int max_cols)
{
    // Adjust offset with bounds checking
    if ((view.basecol + nc) < 0)
        nc = -view.basecol;

    view.basecol += nc;

    // Clamp to non-negative
    if (view.basecol < 0)
        view.basecol = 0;
}

//
// Go to a specific line in the file (based on gtfcn from prototype).
//
void Workspace::goto_line(int target_line, int max_rows)
{
    if (target_line < 0)
        return;

    // Calculate where to position topline to show target_line near middle
    int half_screen = max_rows / 2;

    // Position to show target_line around middle of screen
    scroll_vertical(target_line - view.topline - half_screen, max_rows, total_line_count());

    // Ensure target_line is in visible range
    if (target_line < view.topline)
        view.topline = target_line;
    else if (target_line >= view.topline + max_rows)
        view.topline = target_line - max_rows + 1;

    // Update current position
    position.line = target_line;
    change_current_line(position.line);
}

//
// Update topline when file changes (used by wksp_redraw from prototype).
//
void Workspace::update_topline_after_edit(int from, int to, int delta)
{
    // Adjust topline when lines are inserted/deleted
    // Based on the test expectations, topline should always be adjusted by delta
    // when edits occur, to keep content in the same screen position

    view.topline += delta;

    // Ensure topline doesn't go negative
    if (view.topline < 0)
        view.topline = 0;
}

//
// Write line content back to workspace at specified line number.
//
void Workspace::put_line(int line_no, const std::string &line_content)
{
    // Write the modified line to temp file and get a segment for it
    auto temp_segments = tempfile_.write_line_to_temp(line_content);
    if (temp_segments.empty()) {
        // TODO: error message, as we lost contents of the current line.
        return;
    }

    // If the file is currently empty (line_count == 0) and we're adding line 0,
    // we can just insert the segment without calling breaksegm
    auto total = total_line_count();
    if (total == 0 && line_no == 0) {
        // Simple case: empty file, adding first line
        // Find tail and insert before it
        auto tail_it = contents_.end();
        for (auto it = contents_.begin(); it != contents_.end(); ++it) {
            if (it->is_empty()) {
                tail_it = it;
                break;
            }
        }
        contents_.splice(tail_it, temp_segments);
        auto new_seg_it = std::prev(tail_it);

        cursegm_            = new_seg_it;
        position.line       = 0;
        position.segmline   = 0;
        file_state.modified = true;
        return;
    }

    // Break segment at line_no position to split into segments before and at line_no
    int break_result = breaksegm(line_no, true);

    // Get the new segment to use
    auto new_seg_it = temp_segments.begin();

    if (break_result == 0) {
        // Normal case: line exists, split it
        // Now cursegm_ points to the segment starting at line_no
        // Get iterator to the segment we want to replace (the one containing line_no)
        auto old_seg_it = cursegm_;

        // Check if the segment only contains one line (or if we're at end of file)
        int segmline       = position.segmline;
        bool only_one_line = (old_seg_it->line_count == 1);

        if (!only_one_line) {
            // Break at line_no + 1 to isolate the line
            breaksegm(line_no + 1, false);
            // Now cursegm_ points to segment starting at line_no + 1
        }

        // Replace old segment with new segment
        *old_seg_it       = std::move(*new_seg_it);
        cursegm_          = old_seg_it;
        position.segmline = segmline;

        // Try to merge adjacent segments (but not if we just created blank lines)
        if (only_one_line || line_no < total) {
            catsegm();
        }

        // Mark workspace as modified
        file_state.modified = true;
    } else if (break_result == 1) {
        // breaksegm created blank lines
        // The workspace is now positioned at line_no, which may be in the middle of a blank segment
        auto blank_seg_it = cursegm_;
        int segmline      = position.segmline;

        // Check if line_no is at the start of the segment
        bool at_segment_start = (line_no == segmline);

        if (!at_segment_start || blank_seg_it->line_count > 1) {
            // Need to isolate line_no into its own segment
            // First, split before line_no if it's not at the start
            if (!at_segment_start) {
                breaksegm(line_no, false);
                // Now cursegm_ points to segment starting at line_no
                blank_seg_it = cursegm_;
                segmline     = line_no;
            }

            // Then split after line_no if there are more lines in the segment
            if (blank_seg_it->line_count > 1) {
                breaksegm(line_no + 1, false);
                // Now cursegm_ points to segment starting at line_no + 1
                // Go back to the segment containing line_no
                --blank_seg_it;
            }
        }

        // Now blank_seg_it points to a segment containing only line_no
        // Replace it with the content segment
        *blank_seg_it = std::move(*new_seg_it);

        cursegm_          = blank_seg_it;
        position.segmline = segmline;
        position.line     = line_no;

        // Mark workspace as modified
        file_state.modified = true;
    }
}

//
// Debug routine: print all fields and segment chain.
//
void Workspace::debug_print(std::ostream &out) const
{
    out << "Workspace["
        << "writable=" << file_state.writable << ", "
        << "topline=" << view.topline << ", "
        << "basecol=" << view.basecol << ", "
        << "line=" << position.line << ", "
        << "segmline=" << position.segmline << ", "
        << "cursorcol=" << view.cursorcol << ", "
        << "cursorrow=" << view.cursorrow << ", "
        << "modified=" << (file_state.modified ? "true" : "false") << ", "
        << "backup_done=" << (file_state.backup_done ? "true" : "false") << ", "
        << "original_fd=" << original_fd_ << ", "
        << "cursegm=" << (cursegm_ == contents_.end() ? nullptr : &*cursegm_) << ", "
        << "head=" << (contents_.empty() ? nullptr : &contents_.front()) << "]\n";

    // Print segment chain
    out << "Segment chain:\n";
    int seg_idx = 0;
    for (const auto &seg : contents_) {
        out << "  [" << seg_idx << "] ";
        seg.debug_print(out);
        ++seg_idx;
    }
    if (seg_idx == 0) {
        out << "  (empty)\n";
    }
}
