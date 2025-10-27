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
    head_    = new Segment();
    cursegm_ = head_;
}

Workspace::~Workspace()
{
    cleanup_segments();
}

void Workspace::cleanup_segments()
{
    if (head_) {
        Segment *seg = head_;
        while (seg) {
            Segment *next = seg->next;
            delete seg;
            seg = next;
        }
        head_    = nullptr;
        cursegm_ = nullptr;
    }
}

void Workspace::reset()
{
    cleanup_segments();
    writable_    = 0;
    nlines_      = 0;
    topline_     = 0;
    basecol_     = 0;
    line_        = 0;
    segmline_    = 0;
    cursorcol_   = 0;
    cursorrow_   = 0;
    modified_    = false;
    backup_done_ = false;
}

//
// Set the segment chain for the workspace.
// Deallocates all segments of the previous chain including the tail segment,
// then sets the new chain and appends a new tail if missing.
//
void Workspace::set_chain(Segment *chain)
{
    // Set new chain
    // TODO: fix memory leak, deallocate old chain first
    head_    = chain;
    cursegm_ = head_;

    if (!head_) {
        // Create an empty chain with just a tail
        head_     = new Segment();
        cursegm_  = head_;
        return;
    }

    // Find the last segment in the new chain
    Segment *last = head_;
    while (last->next && last->next->fdesc != 0) {
        last = last->next;
    }

    // Check if the chain already ends with a tail (fdesc == 0)
    bool has_tail = (last->fdesc == 0) || (last->next && last->next->fdesc == 0);

    // If no tail exists, append a new one
    if (!has_tail) {
        Segment *tail = new Segment();
        tail->prev    = last;
        last->next    = tail;
    }
}

//
// Build segment chain from in-memory lines vector.
// Writes lines into temp file.
//
void Workspace::build_segments_from_lines(const std::vector<std::string> &lines)
{
    reset();
    writable_ = 1;
    nlines_   = lines.size();

    if (nlines_ > 0) {
        // Write lines to temp file and get a segment
        head_ = tempfile_.write_lines_to_temp(lines);
        if (!head_)
            throw std::runtime_error(
                "build_segments_from_lines: failed to write lines to temp file");
    } else {
        // Empty file - create a tail marker
        head_ = new Segment();
    }
    cursegm_ = head_;
}

//
// Parse text and build segment chain from it.
//
void Workspace::build_segments_from_text(const std::string &text)
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
    build_segments_from_lines(lines_vec);
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

    if (!cursegm_)
        throw std::runtime_error("set_current_segment: empty workspace");

    // Move forward to find the segment containing lno
    while (lno >= segmline_ + cursegm_->nlines) {
        if (cursegm_->fdesc == 0) {
            // Hit tail segment - line is beyond end of file
            // Return 1 to signal that line is beyond end of file
            line_ = segmline_;
            return 1;
        }
        segmline_ += cursegm_->nlines;

        // Check if there's a next segment before moving
        if (!cursegm_->next) {
            throw std::runtime_error("set_current_segment: null segment in chain");
        }
        cursegm_ = cursegm_->next;
    }

    // Move backward to find the segment containing lno
    while (lno < segmline_) {
        if (!cursegm_->prev)
            throw std::runtime_error("set_current_segment: null previous segment");
        cursegm_ = cursegm_->prev;
        segmline_ -= cursegm_->nlines;
    }

    // Consistency check: segmline_ should not be negative
    if (segmline_ < 0) {
        throw std::runtime_error("set_current_segment: line count lost (segmline_ < 0)");
    }

    // Update workspace state
    line_ = lno;

    return 0;
}

//
// Load file content into segment chain structure.
//
void Workspace::load_file_to_segments(const std::string &path)
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
    build_segments_from_file(fd);
}

//
// Build segment chain from file descriptor.
//
void Workspace::build_segments_from_file(int fd)
{
    nlines_ = 0;

    // Clean up old chain
    cleanup_segments();

    // Build segment chain by reading file
    Segment *first_seg = nullptr;
    Segment *last_seg  = nullptr;

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
                    // Create final segment
                    Segment *seg = new Segment();
                    seg->prev    = last_seg;
                    seg->next    = nullptr;
                    seg->nlines  = lines_in_seg;
                    seg->fdesc   = fd;
                    seg->seek    = seg_seek;
                    seg->sizes   = std::move(temp_seg.sizes);

                    if (last_seg)
                        last_seg->next = seg;
                    else
                        first_seg = seg;

                    last_seg = seg;
                    nlines_ += lines_in_seg;
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
            Segment *seg = new Segment();
            seg->prev    = last_seg;
            seg->next    = nullptr;
            seg->nlines  = lines_in_seg;
            seg->fdesc   = fd;
            seg->seek    = seg_seek;
            seg->sizes   = std::move(temp_seg.sizes);

            if (last_seg)
                last_seg->next = seg;
            else
                first_seg = seg;

            last_seg = seg;
            nlines_ += lines_in_seg;

            lines_in_seg = 0;
        }
    }

    // Create tail segment
    if (first_seg) {
        Segment *tail = new Segment();
        tail->prev    = last_seg;
        tail->seek    = file_offset;

        if (last_seg)
            last_seg->next = tail;
        else
            first_seg = tail;
    } else {
        // Empty file - create a tail segment
        first_seg = new Segment();
    }

    head_     = first_seg;
    cursegm_  = first_seg;
    segmline_ = 0;
    line_     = 0;
}

//
// Read line content from segment chain at specified index.
//
std::string Workspace::read_line_from_segment(int line_no)
{
    if (set_current_segment(line_no)) {
        // Beyond end of file.
        return "";
    }

    Segment *seg = cursegm_;
    int rel_line = line_no - segmline_;

    // Calculate file offset by accumulating line lengths
    // Note: seg->seek points to the START of the first line in the segment
    // We need to skip 'rel_line' lines to get to the line we want
    long seek_pos = seg->seek;
    for (int i = 0; i < rel_line; ++i) {
        int len = seg->sizes[i];
        seek_pos += len;
    }

    // Get line length
    int line_len = seg->sizes[rel_line];
    if (line_len <= 0 || !seg->has_contents())
        return "";

    // Handle empty lines (just newline) - return empty string without newline
    if (line_len == 1 || seg->fdesc < 0) {
        return "";
    }

    // Read line from file
    std::string result(line_len - 1, '\0'); // exclude newline
    if (lseek(seg->fdesc, seek_pos, SEEK_SET) >= 0) {
        read(seg->fdesc, result.data(), result.size());
    }
    return result;
}

//
// Write segment chain content to file.
//
bool Workspace::write_segments_to_file(const std::string &path)
{
    if (!head_) {
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

    Segment *seg = head_;
    char buffer[8192];

    while (seg && seg->has_contents()) {
        // Calculate total bytes for this segment
        long total_bytes = seg->get_total_bytes();

        if (seg->fdesc > 0) {
            // Read from source file and write to output
            if (lseek(seg->fdesc, seg->seek, SEEK_SET) < 0) {
                // Failed to seek - file may have been unlinked
                // Skip this segment and continue
                seg = seg->next;
                continue;
            }
            while (total_bytes > 0) {
                int to_read = (total_bytes < (long)sizeof(buffer)) ? total_bytes : sizeof(buffer);
                int nread   = read(seg->fdesc, buffer, to_read);
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

        seg = seg->next;
    }

    close(out_fd);
    return true;
}

//
// Get total line count. Returns workspace line count if segments exist,
// otherwise returns the fallback count from the lines vector.
//
int Workspace::get_line_count(int fallback_count) const
{
    return head_ ? nlines_ : fallback_count;
}

//
// Split segment at given line number (based on breaksegm from prototype).
// When the needed line is beyond the end of file, creates empty segments
// with blank lines.
// Returns 0 on success, 1 if blank lines were appended.
//
int Workspace::breaksegm(int line_no, bool realloc_flag)
{
    if (!head_ || !cursegm_)
        throw std::runtime_error("breaksegm: empty workspace");

    // Position workspace to the target line
    if (set_current_segment(line_no)) {
        // Line is beyond end of file - create blank lines to extend it
        // This matches the prototype behavior
        // Calculate how many blank lines to create
        // line_ was set by set_current_segment to segmline_ of the tail segment
        // Check if workspace is empty (only has a tail segment)
        bool is_empty = (head_->fdesc == 0);

        int num_blank_lines;

        if (is_empty) {
            // Empty file, create lines from 0 to line_no
            num_blank_lines = line_no + 1;
        } else {
            // Normal case: create lines from line_ to line_no
            num_blank_lines = (line_no - line_) + 1;
        }

        // Must create at least 1 blank line
        if (num_blank_lines <= 0) {
            num_blank_lines = 1;
        }

        Segment *blank_seg = create_blank_lines(num_blank_lines);
        if (!blank_seg)
            throw std::runtime_error("breaksegm: failed to create blank lines");

        // Get the tail segment (the one we're currently at, beyond EOF)
        Segment *tail = cursegm_;
        Segment *prev = tail ? tail->prev : nullptr;

        // Remove tail temporarily (set_chain will create a new one)
        if (prev)
            prev->next = nullptr;

        // Find the last blank segment and remove the tail that was created by create_blank_lines
        Segment *last_blank = blank_seg;
        while (last_blank->next) {
            if (last_blank->next->fdesc == 0) {
                // Found the tail created by create_blank_lines - remove it
                Segment *extra_tail = last_blank->next;
                last_blank->next    = nullptr;
                extra_tail->prev    = nullptr;
                delete extra_tail;
                break;
            }
            last_blank = last_blank->next;
        }

        // Update workspace
        if (prev) {
            // Link blank segments to existing chain and keep the old tail
            blank_seg->prev = prev;
            prev->next      = blank_seg;

            // Link tail to the end of blank segments
            last_blank->next = tail;
            tail->prev       = last_blank;
        } else {
            // No existing chain - set_chain will create a new tail
            set_chain(blank_seg);
        }

        // Calculate where the blank segments start
        // line_ is the segmline_ of the tail, which is the first position beyond the last valid
        // line For empty: line_=0, so blank segments start at 0 For non-empty: line_ is the first
        // position beyond valid, so blank segments start at line_
        int blank_seg_start = line_;

        // Position to the first blank segment
        cursegm_  = blank_seg;
        segmline_ = blank_seg_start;

        // Update nlines to include the blank lines
        nlines_ = line_no + 1;

        return 1; // Signal that we created lines
    }

    // Now we're at the segment containing line_no
    Segment *seg = cursegm_;
    int rel_line = line_no - segmline_;

    if (rel_line == 0) {
        return 0; // Already at the right position
    }

    // Special case: blank line segment (fdesc == -1) - split by sizes array
    if (seg->fdesc == -1) {
        if (rel_line >= seg->nlines) {
            throw std::runtime_error(
                "breaksegm: inconsistent rel_line after set_current_segment()");
        }

        // Split the blank line segment
        Segment *new_seg = new Segment();
        new_seg->nlines  = seg->nlines - rel_line;
        new_seg->fdesc   = -1; // Still blank lines
        new_seg->seek    = seg->seek;

        // Copy remaining sizes
        for (size_t i = rel_line; i < seg->sizes.size(); ++i) {
            new_seg->sizes.push_back(seg->sizes[i]);
        }

        // Link new segment
        new_seg->next = seg->next;
        new_seg->prev = seg;
        if (seg->next)
            seg->next->prev = new_seg;
        seg->next = new_seg;

        // Truncate original segment sizes
        seg->sizes.resize(rel_line);
        seg->nlines = rel_line;

        // Update workspace position
        cursegm_  = new_seg;
        segmline_ = line_no;

        return 0;
    }

    // Normal file segment - record where we are in the data
    size_t split_point = rel_line;

    // Walk through the first rel_line lines to calculate offset
    long offs = 0;
    for (int i = 0; i < rel_line; ++i) {
        if (i >= seg->nlines) {
            split_point = seg->nlines;
            break;
        }
        offs += seg->sizes[i];
    }

    // Create new segment for the remainder
    Segment *new_seg = new Segment();
    new_seg->nlines  = seg->nlines - rel_line;
    new_seg->fdesc   = seg->fdesc;
    new_seg->seek    = seg->seek + offs;

    // Copy remaining data bytes from split_point to end
    for (size_t i = split_point; i < seg->nlines; ++i) {
        new_seg->sizes.push_back(seg->sizes[i]);
    }

    // Link new segment
    new_seg->next = seg->next;
    new_seg->prev = seg;
    if (seg->next)
        seg->next->prev = new_seg;
    seg->next = new_seg;

    // Truncate original segment data - keep only first rel_line data
    std::vector<unsigned short> new_sizes;
    for (size_t i = 0; i < split_point; ++i) {
        new_sizes.push_back(seg->sizes[i]);
    }
    seg->sizes  = new_sizes;
    seg->nlines = rel_line;

    // Update workspace position
    cursegm_  = new_seg;
    segmline_ = line_no;

    return 0;
}

//
// Merge adjacent segments (based on catsegm from prototype).
// Tries to merge current segment with previous if they're adjacent.
// Returns true if merge occurred, false otherwise.
//
bool Workspace::catsegm()
{
    if (!head_ || !cursegm_ || !cursegm_->prev)
        return false;

    Segment *curr = cursegm_;
    Segment *prev = curr->prev;

    // Check if segments can be merged
    // They must be from the same file (not tail segments) and together have < 127 lines
    if (prev->fdesc > 0 && prev->fdesc == curr->fdesc && (prev->nlines + curr->nlines) < 127) {
        // Calculate if they're adjacent
        long prev_bytes = prev->get_total_bytes();
        if (curr->seek == prev->seek + prev_bytes) {
            // Segments are adjacent - merge them
            Segment *merged = new Segment();
            merged->nlines  = prev->nlines + curr->nlines;
            merged->fdesc   = prev->fdesc;
            merged->seek    = prev->seek;
            merged->next    = curr->next;
            merged->prev    = prev->prev;

            // Copy data from both segments
            merged->sizes = prev->sizes;
            for (unsigned char byte : curr->sizes)
                merged->sizes.push_back(byte);

            // Update links
            if (prev->prev)
                prev->prev->next = merged;
            else
                head_ = merged;

            if (curr->next)
                curr->next->prev = merged;

            // Update workspace position
            if (cursegm_ == curr) {
                cursegm_ = merged;
                segmline_ -= prev->nlines;
            }

            // Clean up old segments
            delete prev;
            delete curr;

            return true;
        }
    }

    return false;
}

//
// Insert segments into workspace before given line (based on insert from prototype).
//
void Workspace::insert_segments(Segment *new_seg, int at)
{
    if (!new_seg)
        return;

    // Find the last segment in the chain to insert
    Segment *last      = new_seg;
    int inserted_lines = 0;
    while (last->next && last->next->fdesc != 0) {
        inserted_lines += last->nlines;
        last = last->next;
    }
    inserted_lines += last->nlines;

    // Split at insertion point
    if (breaksegm(at, true) == 0) {
        // after breaksegm, cursegm_ points to the segment at position 'at'
        // Link new segments BEFORE cursegm_
        Segment *insert_before = cursegm_;            // The segment at position 'at'
        Segment *insert_prev   = insert_before->prev; // Segment before position 'at'

        // Insert new_seg before insert_before
        new_seg->prev       = insert_prev;
        new_seg->next       = insert_before;
        insert_before->prev = last;
        last->next          = insert_before;

        if (insert_prev)
            insert_prev->next = new_seg;
        else
            head_ = new_seg; // New segments become the start

        // Update workspace position
        cursegm_  = new_seg;
        segmline_ = at;

        // Update line count
        nlines_ += inserted_lines;
    }

    writable_ = 1; // Mark as edited
}

//
// Delete segments between from and to lines (based on delete from prototype).
// Returns the deleted segment chain.
//
Segment *Workspace::delete_segments(int from, int to)
{
    if (!head_ || from > to)
        return nullptr;

    // Empty workspace has no content to delete
    if (nlines_ == 0)
        return nullptr;

    // Break at line 'to' (not to+1) to get the segment containing line 'to'
    // After this, cursegm_ points to the segment containing line 'to'
    int result = breaksegm(to, true);
    if (result != 0) {
        // Line doesn't exist - allow deletion to last line
        if (to + 1 > nlines_) {
            // Just move to the last line position
            set_current_segment(nlines_);
        } else {
            return nullptr;
        }
    }

    // After breaksegm(to), cursegm_ points to the segment starting at line 'to'
    // We want to delete up to and including 'to', so we need to go to 'to+1'
    // Find the segment AFTER 'to'
    Segment *end_seg = cursegm_;
    Segment *after   = end_seg->next; // Save pointer to what comes after

    // Now position to 'to+1' to get the segment after the deletion range
    if (to + 1 < nlines_) {
        if (breaksegm(to + 1, true) == 0) {
            after = cursegm_;
        }
    } else {
        // We're deleting to the end
        after = nullptr;
    }

    // Break at start line
    result = breaksegm(from, true);
    if (result != 0) {
        // Could not break at start line - might be out of bounds
        return nullptr;
    }

    Segment *start_seg = cursegm_;
    Segment *before    = start_seg->prev;

    // Unlink the segment chain to delete
    if (before)
        before->next = after;
    else
        head_ = after;

    if (after)
        after->prev = before;

    // Update workspace position
    cursegm_ = after ? after : before;
    if (cursegm_ == after) {
        segmline_ = from;
    } else if (cursegm_ == before) {
        segmline_ = segmline_ - (to - from + 1);
    }

    // Remove the back link from the deleted chain
    start_seg->prev = nullptr;

    // Detach end_seg from the main chain (don't create a tail for deleted segments)
    end_seg->next = nullptr;

    // Count lines in deleted segments
    int deleted_lines = 0;
    Segment *del_seg  = start_seg;
    while (del_seg) {
        deleted_lines += del_seg->nlines;
        del_seg = del_seg->next;
    }

    // Update line count
    nlines_ -= deleted_lines;

    writable_ = 1; // Mark as edited

    return start_seg;
}

//
// Copy segment chain (based on copysegm from prototype).
// Returns a deep copy of the segment chain from start to end.
//
Segment *Workspace::copy_segment_chain(Segment *start, Segment *end)
{
    if (!start)
        return nullptr;

    Segment *copied_first = nullptr;
    Segment *copied_last  = nullptr;

    Segment *curr = start;
    while (curr && curr != end) {
        Segment *copy = new Segment();
        copy->nlines  = curr->nlines;
        copy->fdesc   = curr->fdesc;
        copy->seek    = curr->seek;
        copy->sizes   = curr->sizes; // Copy vector
        copy->next    = nullptr;
        copy->prev    = copied_last;

        if (copied_last)
            copied_last->next = copy;
        else
            copied_first = copy;

        copied_last = copy;

        if (curr->fdesc == 0)
            break;

        curr = curr->next;
    }

    // Note: we do NOT add a tail segment here - it should already exist in the workspace

    return copied_first;
}

//
// Create segments for n empty lines (based on blanklines from prototype).
// Each empty line has length 1 (just the newline).
//
Segment *Workspace::create_blank_lines(int n)
{
    if (n <= 0)
        return nullptr;

    Segment *first = nullptr;
    Segment *last  = nullptr;

    while (n > 0) {
        int lines_in_seg = (n > 127) ? 127 : n;

        Segment *seg = new Segment();
        seg->nlines  = lines_in_seg;
        seg->fdesc   = -1; // Empty lines not from file
        seg->seek    = 0;

        // Add line length data: each empty line has length 1 (just newline)
        seg->sizes.resize(lines_in_seg);
        for (int i = 0; i < lines_in_seg; ++i) {
            seg->sizes[i] = 1;
        }

        // Link segments
        seg->prev = last;
        if (last)
            last->next = seg;
        else
            first = seg;

        last = seg;
        n -= lines_in_seg;
    }

    // Note: we do NOT add a tail segment here - it should already exist in the workspace

    return first;
}

//
// Cleanup a segment chain (static helper for tests).
//
void Workspace::cleanup_segments(Segment *seg)
{
    if (!seg)
        return;

    // First, unlink from chain
    if (seg->prev)
        seg->prev->next = nullptr;

    // Delete all segments in chain
    while (seg) {
        Segment *next = seg->next;
        delete seg;
        seg = next;
    }
}

//
// Scroll workspace by nl lines (based on wksp_forward from prototype).
// nl: negative for up, positive for down
//
void Workspace::scroll_vertical(int nl, int max_rows, int total_lines)
{
    if (nl < 0) {
        // Scroll up (toward beginning)
        if (topline_ == 0) {
            // Already at top
            return;
        }
    } else {
        // Scroll down (toward end)
        int last_line = total_lines - topline_;
        if (last_line <= max_rows) {
            // Already showing the end
            return;
        }
    }

    topline_ += nl;

    // Clamp topline to valid range
    if (topline_ > total_lines - max_rows)
        topline_ = total_lines - max_rows;
    if (topline_ < 0)
        topline_ = 0;

    // Update current line to stay in visible range
    if (line_ > topline_ + max_rows - 1)
        line_ = topline_ + max_rows - 1;
    if (line_ < topline_)
        line_ = topline_;
}

//
// Shift horizontal view by nc columns (based on wksp_offset from prototype).
// nc: negative for left, positive for right
//
void Workspace::scroll_horizontal(int nc, int max_cols)
{
    // Adjust offset with bounds checking
    if ((basecol_ + nc) < 0)
        nc = -basecol_;

    basecol_ += nc;

    // Clamp to non-negative
    if (basecol_ < 0)
        basecol_ = 0;
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
    scroll_vertical(target_line - topline_ - half_screen, max_rows, nlines_);

    // Ensure target_line is in visible range
    if (target_line < topline_)
        topline_ = target_line;
    else if (target_line >= topline_ + max_rows)
        topline_ = target_line - max_rows + 1;

    // Update current position
    line_ = target_line;
    set_current_segment(line_);
}

//
// Update topline when file changes (used by wksp_redraw from prototype).
//
void Workspace::update_topline_after_edit(int from, int to, int delta)
{
    // Adjust topline when lines are inserted/deleted
    int j = (delta >= 0) ? to : from;

    if (topline_ > j) {
        topline_ += delta;

        // Ensure topline doesn't go negative
        if (topline_ < 0)
            topline_ = 0;
    }
}

//
// Debug routine: print all fields and segment chain.
//
void Workspace::debug_print(std::ostream &out) const
{
    out << "Workspace["
        << "writable_=" << writable_ << ", "
        << "nlines_=" << nlines_ << ", "
        << "topline_=" << topline_ << ", "
        << "basecol_=" << basecol_ << ", "
        << "line_=" << line_ << ", "
        << "segmline_=" << segmline_ << ", "
        << "cursorcol_=" << cursorcol_ << ", "
        << "cursorrow_=" << cursorrow_ << ", "
        << "modified_=" << (modified_ ? "true" : "false") << ", "
        << "backup_done_=" << (backup_done_ ? "true" : "false") << ", "
        << "original_fd_=" << original_fd_ << ", "
        << "cursegm_=" << static_cast<void *>(cursegm_) << ", "
        << "head_=" << static_cast<void *>(head_) << "]\n";

    // Print segment chain
    out << "Segment chain:\n";
    int seg_idx  = 0;
    Segment *seg = head_;
    while (seg) {
        out << "  [" << seg_idx << "] ";
        seg->debug_print(out);
        seg = seg->next;
        ++seg_idx;
    }
    if (seg_idx == 0) {
        out << "  (empty)\n";
    }
}
