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
    segments_.emplace_back();
    cursegm_ = segments_.begin();
}

Workspace::~Workspace()
{
    cleanup_segments();
}

void Workspace::cleanup_segments()
{
    segments_.clear();
    cursegm_ = segments_.end();
}

void Workspace::reset()
{
    cleanup_segments();
    file_state.writable    = 0;
    file_state.nlines      = 0;
    view.topline           = 0;
    view.basecol           = 0;
    position.line          = 0;
    position.segmline      = 0;
    view.cursorcol         = 0;
    view.cursorrow         = 0;
    file_state.modified    = false;
    file_state.backup_done = false;

    // Create initial tail segment (empty workspace still has a tail)
    segments_.emplace_back();
    cursegm_ = segments_.begin();
}

//
// Set the segment chain for the workspace.
// Copies the provided list of segments to the internal std::list.
// This method is used for compatibility with external segment chains during transition.
//
void Workspace::set_chain(std::list<Segment> &segments)
{
    if (segments.empty())
        return;

    // Clear existing segments but keep the tail structure
    this->segments_.clear();
    cursegm_ = this->segments_.end();

    // Copy all segments from the input list
    for (const auto &seg : segments) {
        this->segments_.emplace_back(seg); // Copy segment data
    }

    file_state.nlines = 0;
    for (const auto &seg : this->segments_) {
        if (seg.fdesc == 0)
            break; // Don't count tail segment
        file_state.nlines += seg.nlines;
    }

    // Ensure we have a tail segment
    if (this->segments_.empty() || this->segments_.back().fdesc != 0) {
        this->segments_.emplace_back();
    }

    // Set cursegm_ to the first non-tail segment
    cursegm_ = this->segments_.begin();
    if (!this->segments_.empty() && this->segments_.front().fdesc == 0) {
        // Only tail - set to end
        cursegm_ = this->segments_.end();
    }
}

//
// Build segment chain from in-memory lines vector.
// Writes lines into temp file.
//
void Workspace::load_text(const std::vector<std::string> &lines)
{
    reset();
    file_state.writable = 1;
    file_state.nlines   = lines.size();

    if (file_state.nlines > 0) {
        // Write lines to temp file and get a segment
        auto segments_from_temp = tempfile_.write_lines_to_temp(lines);
        if (segments_from_temp.empty())
            throw std::runtime_error("load_text: failed to write lines to temp file");
        // Move the segment into our segments_ list BEFORE the tail
        // reset() already created a tail segment, so insert before it
        auto insert_pos = std::prev(segments_.end());
        segments_.splice(insert_pos, segments_from_temp);
    }

    // Tail segment already exists from reset(), no need to add another

    // Set cursegm_ to the first segment (the content, not the tail)
    cursegm_          = segments_.begin();
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
int Workspace::set_current_segment(int lno)
{
    // Validate input: negative line numbers are invalid
    if (lno < 0)
        throw std::runtime_error("set_current_segment: negative line number");

    if (cursegm_ == segments_.end())
        throw std::runtime_error("set_current_segment: empty workspace");

    // Move forward to find the segment containing lno
    while (lno >= position.segmline + cursegm_->nlines) {
        if (cursegm_->fdesc == 0) {
            // Hit tail segment - line is beyond end of file
            // Return 1 to signal that line is beyond end of file
            position.line = position.segmline;
            return 1;
        }
        position.segmline += cursegm_->nlines;

        // Check if there's a next segment before moving
        auto next_it = std::next(cursegm_);
        if (next_it == segments_.end()) {
            throw std::runtime_error("set_current_segment: null segment in chain");
        }
        ++cursegm_;
    }

    // Move backward to find the segment containing lno
    while (lno < position.segmline) {
        if (cursegm_ == segments_.begin())
            throw std::runtime_error("set_current_segment: null previous segment");
        --cursegm_;
        position.segmline -= cursegm_->nlines;
    }

    // Consistency check: segmline should not be negative
    if (position.segmline < 0) {
        throw std::runtime_error("set_current_segment: line count lost (segmline < 0)");
    }

    // Update workspace state
    position.line = lno;

    return 0;
}

//
// Load file content into segment chain structure.
//
void Workspace::load_file(const std::string &path, bool create_if_missing)
{
    // Open file for reading
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return;
    }

    // Store the original fd so we can use it later
    original_fd_ = fd;

    // Build segment chain from file
    // Note: we keep the fd open because segments reference it via fdesc
    load_file(fd);
}

//
// Build segment chain from file descriptor.
//
void Workspace::load_file(int fd)
{
    file_state.nlines = 0;

    // Clean up old chain
    cleanup_segments();

    // Build segment chain by reading file
    // Clear any existing segments list
    segments_.clear();

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
                    segments_.emplace_back();
                    Segment &seg = segments_.back();
                    seg.nlines   = lines_in_seg;
                    seg.fdesc    = fd;
                    seg.seek     = seg_seek;
                    seg.sizes    = std::move(temp_seg.sizes);

                    file_state.nlines += lines_in_seg;
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

        temp_seg.sizes.push_back(line_len);

        ++lines_in_seg;
        file_offset += line_len;

        // Create new segment if we've hit limits
        if (lines_in_seg >= 127 || temp_seg.sizes.size() >= 4000) {
            segments_.emplace_back();
            Segment &seg = segments_.back();
            seg.nlines   = lines_in_seg;
            seg.fdesc    = fd;
            seg.seek     = seg_seek;
            seg.sizes    = std::move(temp_seg.sizes);

            file_state.nlines += lines_in_seg;

            lines_in_seg = 0;
        }
    }

    // Create tail segment if needed
    if (!segments_.empty() && segments_.back().fdesc != 0) {
        segments_.emplace_back();
    }

    cursegm_          = segments_.begin();
    position.segmline = 0;
    position.line     = 0;
}

//
// Copy segment list (based on copysegm from prototype).
// Returns a deep copy of the segment list from start to end.
//
std::list<Segment> Workspace::copy_segment_list(Segment::iterator start, Segment::iterator end)
{
    std::list<Segment> copied_segments;

    for (auto it = start; it != end; ++it) {
        Segment copy;
        copy.nlines = it->nlines;
        copy.fdesc  = it->fdesc;
        copy.seek   = it->seek;
        copy.sizes  = it->sizes; // Copy vector

        copied_segments.push_back(copy);

        if (it->fdesc == 0)
            break;
    }

    return copied_segments;
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
        seg.nlines = lines_in_seg;
        seg.fdesc  = -1; // Empty lines not from file
        seg.seek   = 0;

        // Add line length data: each empty line has length 1 (just newline)
        seg.sizes.resize(lines_in_seg);
        for (int i = 0; i < lines_in_seg; ++i) {
            seg.sizes[i] = 1;
        }

        segments.push_back(seg);
        n -= lines_in_seg;
    }

    // Note: we do NOT add a tail segment here - it should already exist in the workspace

    return segments;
}

//
// Cleanup a segment list (static helper for tests).
// Note: With std::list<Segment>, manual cleanup is not needed since destructors handle it.
// This method is now a no-op kept for API compatibility during transition.
//
void Workspace::cleanup_segments(std::list<Segment> &segments)
{
    segments.clear();
}

//
// Read line content from segment chain at specified index.
// Enhanced version using iterator instead of Segment* pointer for safer access and modern C++
// practices.
//
std::string Workspace::read_line_from_segment(int line_no)
{
    // Validate iterator is valid and points to a segment
    if (cursegm_ == segments_.end()) {
        return "";
    }

    // Verify the segment has contents (not a tail segment)
    if (!cursegm_->has_contents()) {
        return "";
    }

    // Calculate relative line position within the current segment
    int rel_line = line_no - position.segmline;

    // Bounds checking: ensure rel_line is within segment bounds
    if (rel_line < 0 || rel_line >= static_cast<int>(cursegm_->sizes.size())) {
        return ""; // Line index out of bounds for this segment
    }

    // Calculate file offset by accumulating line lengths
    // Note: cursegm_->seek points to the START of the first line in the segment
    // We need to skip 'rel_line' lines to get to the line we want
    long seek_pos = cursegm_->seek;
    for (int i = 0; i < rel_line; ++i) {
        seek_pos += cursegm_->sizes[i];
    }

    // Get line length for the requested line
    int line_len = cursegm_->sizes[rel_line];
    if (line_len <= 0) {
        return "";
    }

    // Handle empty lines (just newline) - return empty string without newline
    if (line_len == 1 || cursegm_->fdesc < 0) {
        return "";
    }

    // Read line from file using iterator's segment data
    std::string result(line_len - 1, '\0'); // exclude newline
    if (lseek(cursegm_->fdesc, seek_pos, SEEK_SET) >= 0) {
        read(cursegm_->fdesc, static_cast<void *>(&result[0]), result.size());
    }
    return result;
}

//
// Write segment chain content to file.
//
bool Workspace::write_segments_to_file(const std::string &path)
{
    if (segments_.empty()) {
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

    for (const auto &seg : segments_) {
        if (!seg.has_contents())
            continue; // Skip tail segment

        // Calculate total bytes for this segment
        long total_bytes = seg.get_total_bytes();

        if (seg.fdesc > 0) {
            // Read from source file and write to output
            if (lseek(seg.fdesc, seg.seek, SEEK_SET) < 0) {
                // Failed to seek - file may have been unlinked
                // Skip this segment and continue
                continue;
            }
            while (total_bytes > 0) {
                int to_read = (total_bytes < (long)sizeof(buffer)) ? total_bytes : sizeof(buffer);
                int nread   = read(seg.fdesc, buffer, to_read);
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
    if (segments_.empty())
        throw std::runtime_error("breaksegm: empty workspace");

    // Position workspace to the target line
    if (set_current_segment(line_no)) {
        // Line is beyond end of file - create blank lines to extend it
        // This matches the prototype behavior
        // Calculate how many blank lines to create
        // position.line was set by set_current_segment to segmline of the tail segment
        // Check if workspace is empty (only has a tail segment)
        bool is_empty = segments_.front().fdesc == 0;

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

        // Insert blank segments
        segments_.splice(insert_pos, blank_segments);

        // Position to the first blank segment we just inserted
        cursegm_          = std::prev(insert_pos);
        position.segmline = position.line;

        // Update line count
        file_state.nlines = line_no + 1;

        return 1; // Signal that we created lines
    }

    // Now we're at the segment containing line_no
    int rel_line = line_no - position.segmline;

    if (rel_line == 0) {
        return 0; // Already at the right position
    }

    // Special case: blank line segment (fdesc == -1) - split by sizes array
    if (cursegm_->fdesc == -1) {
        if (rel_line >= cursegm_->nlines) {
            throw std::runtime_error(
                "breaksegm: inconsistent rel_line after set_current_segment()");
        }

        // Find where to insert the new segment (after current segment)
        auto insert_pos = std::next(cursegm_);

        // Create new segment in place
        auto new_it      = segments_.insert(insert_pos, Segment());
        Segment &new_seg = *new_it;
        new_seg.nlines   = cursegm_->nlines - rel_line;
        new_seg.fdesc    = -1; // Still blank lines
        new_seg.seek     = cursegm_->seek;

        // Copy remaining sizes
        for (size_t i = rel_line; i < cursegm_->sizes.size(); ++i) {
            new_seg.sizes.push_back(cursegm_->sizes[i]);
        }

        // Truncate original segment sizes
        cursegm_->sizes.resize(rel_line);
        cursegm_->nlines = rel_line;

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
        if (i >= cursegm_->nlines) {
            split_point = cursegm_->nlines;
            break;
        }
        offs += cursegm_->sizes[i];
    }

    // Find where to insert the new segment (after current segment)
    auto insert_pos = std::next(cursegm_);

    // Create new segment in place
    auto new_it      = segments_.insert(insert_pos, Segment());
    Segment &new_seg = *new_it;
    new_seg.nlines   = cursegm_->nlines - rel_line;
    new_seg.fdesc    = cursegm_->fdesc;
    new_seg.seek     = cursegm_->seek + offs;

    // Copy remaining data bytes from split_point to end
    for (size_t i = split_point; i < cursegm_->nlines; ++i) {
        new_seg.sizes.push_back(cursegm_->sizes[i]);
    }

    // Truncate original segment data - keep only first rel_line data
    cursegm_->sizes.resize(split_point);
    cursegm_->nlines = rel_line;

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
    if (cursegm_ == segments_.begin() || cursegm_ == segments_.end())
        return false;

    auto curr_it = cursegm_;
    auto prev_it = std::prev(curr_it);

    Segment &curr = *curr_it;
    Segment &prev = *prev_it;

    // Check if segments can be merged
    // They must be from the same file (not tail segments) and together have < 127 lines
    if (prev.fdesc > 0 && prev.fdesc == curr.fdesc && (prev.nlines + curr.nlines) < 127) {
        // Calculate if they're adjacent
        long prev_bytes = prev.get_total_bytes();
        if (curr.seek == prev.seek + prev_bytes) {
            // Segments are adjacent - merge them
            // Combine data into previous segment
            for (unsigned short byte : curr.sizes)
                prev.sizes.push_back(byte);
            prev.nlines += curr.nlines;

            // Erase current segment from list
            cursegm_ = prev_it;

            // Update workspace position
            position.segmline -= prev.nlines;

            return true;
        }
    }

    return false;
}

//
// Insert segments into workspace before given line (based on insert from prototype).
//
void Workspace::insert_segments(std::list<Segment> &segments_to_insert, int at)
{
    if (segments_to_insert.empty())
        return;

    // Calculate total inserted lines
    int inserted_lines = 0;
    for (const auto &seg : segments_to_insert) {
        inserted_lines += seg.nlines;
    }

    // Split at insertion point
    breaksegm(at, true);

    // after breaksegm, cursegm_ points to the segment at position 'at'
    // Insert new segments BEFORE cursegm_
    auto insert_pos = cursegm_;
    segments_.splice(insert_pos, segments_to_insert);

    // Update workspace position to first inserted segment
    cursegm_          = std::prev(insert_pos);
    position.segmline = at;

    // Update line count
    file_state.nlines += inserted_lines;

    file_state.writable = 1; // Mark as edited
}

//
// Delete segments between from and to lines (based on delete from prototype).
// Returns the deleted segment chain.
//
void Workspace::delete_segments(int from, int to)
{
    // Convert to use list operations - simplified version
    // Original implementation used pointer operations, but we need to use list erase
    if (segments_.empty() || from > to)
        return;

    if (file_state.nlines == 0)
        return;

    // Use breaksegm for positioning (it uses list operations internally now)
    // Break AFTER the last line to delete (to+1) so we can delete up to and including 'to'
    int result = breaksegm(to + 1, true);
    if (result != 0) {
        if (to + 1 > file_state.nlines) {
            set_current_segment(file_state.nlines);
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

    // Calculate deleted lines
    int deleted_lines = 0;
    auto temp_it      = start_delete_it;
    while (temp_it != after_delete_it) {
        deleted_lines += temp_it->nlines;
        ++temp_it;
    }

    // Erase the range from the list
    segments_.erase(start_delete_it, after_delete_it);

    // Update workspace position
    if (!segments_.empty()) {
        cursegm_ =
            (after_delete_it != segments_.end()) ? after_delete_it : std::prev(segments_.end());
        position.segmline = from;
    } else {
        cursegm_          = segments_.begin();
        position.segmline = 0;
    }

    // Update line count
    file_state.nlines -= deleted_lines;

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
    scroll_vertical(target_line - view.topline - half_screen, max_rows, file_state.nlines);

    // Ensure target_line is in visible range
    if (target_line < view.topline)
        view.topline = target_line;
    else if (target_line >= view.topline + max_rows)
        view.topline = target_line - max_rows + 1;

    // Update current position
    position.line = target_line;
    set_current_segment(position.line);
}

//
// Update topline when file changes (used by wksp_redraw from prototype).
//
void Workspace::update_topline_after_edit(int from, int to, int delta)
{
    // Adjust topline when lines are inserted/deleted
    int j = (delta >= 0) ? to : from;

    // For deletions, use >= to handle boundary case
    // For insertions, use > to handle boundary case
    bool should_adjust = (delta >= 0) ? (view.topline > j) : (view.topline >= j);

    if (should_adjust) {
        view.topline += delta;

        // Ensure topline doesn't go negative
        if (view.topline < 0)
            view.topline = 0;
    }
}

//
// Debug routine: print all fields and segment chain.
//
void Workspace::debug_print(std::ostream &out) const
{
    out << "Workspace["
        << "writable=" << file_state.writable << ", "
        << "nlines=" << file_state.nlines << ", "
        << "topline=" << view.topline << ", "
        << "basecol=" << view.basecol << ", "
        << "line=" << position.line << ", "
        << "segmline=" << position.segmline << ", "
        << "cursorcol=" << view.cursorcol << ", "
        << "cursorrow=" << view.cursorrow << ", "
        << "modified=" << (file_state.modified ? "true" : "false") << ", "
        << "backup_done=" << (file_state.backup_done ? "true" : "false") << ", "
        << "original_fd=" << original_fd_ << ", "
        << "cursegm=" << (cursegm_ == segments_.end() ? nullptr : &*cursegm_) << ", "
        << "head=" << (segments_.empty() ? nullptr : &segments_.front()) << "]\n";

    // Print segment chain
    out << "Segment chain:\n";
    int seg_idx = 0;
    for (const auto &seg : segments_) {
        out << "  [" << seg_idx << "] ";
        seg.debug_print(out);
        ++seg_idx;
    }
    if (seg_idx == 0) {
        out << "  (empty)\n";
    }
}
