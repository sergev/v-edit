#include "workspace.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <fstream>

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
        head_   = nullptr;
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
// Build segment chain from in-memory lines vector.
// Writes lines into temp file.
//
void Workspace::build_segments_from_lines(const std::vector<std::string> &lines)
{
    writable_ = 1;
    nlines_   = (int)lines.size();

    // Clean up old chain if any
    cleanup_segments();

    if (nlines_ == 0) {
        // Empty file - create a segment with one empty line in temp file
        Segment *seg = tempfile_.write_lines_to_temp({ "" });
        if (!seg) {
            // Fallback: create an empty segment
            seg         = new Segment();
            seg->prev   = nullptr;
            seg->next   = nullptr;
            seg->nlines = 0;
            seg->fdesc  = 0; // Tail marker
            seg->seek   = 0;
        }
        head_    = seg;
        cursegm_  = seg;
        segmline_ = 0;
        basecol_  = 0;
        line_     = 0;
        return;
    }

    // Use Tempfile to write lines and get a segment
    Segment *seg = tempfile_.write_lines_to_temp(lines);
    if (!seg) {
        return;
    }

    head_    = seg;
    cursegm_  = seg;
    segmline_ = 0;
    basecol_  = 0;
    line_     = 0;
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
// Set workspace to segment containing specified line.
//
int Workspace::position(int lno)
{
    if (!cursegm_)
        return 1;

    Segment *seg = cursegm_;
    int segStart = segmline_;
    // adjust forward
    while (lno >= segStart + seg->nlines && seg->has_contents()) {
        segStart += seg->nlines;
        seg = seg->next;
        if (!seg) {
            // Went past end of chain
            return 1;
        }
    }
    // adjust backward
    while (lno < segStart && seg->prev) {
        seg = seg->prev;
        segStart -= seg->nlines;
    }
    cursegm_  = seg;
    segmline_ = segStart;
    line_     = lno;
    return 0;
}

//
// Compute byte offset of specified line in file.
//
int Workspace::seek(int lno, long &outSeek)
{
    if (position(lno))
        return 1;
    Segment *seg = cursegm_;
    long seek    = (long)seg->seek;
    int rel      = lno - segmline_;
    for (int i = 0; i < rel; ++i) {
        int len = seg->sizes[i];
        seek += len;
    }
    outSeek = seek;
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
        tail->next    = nullptr;
        tail->nlines  = 0;
        tail->fdesc   = 0;
        tail->seek    = file_offset;

        if (last_seg)
            last_seg->next = tail;
        else
            first_seg = tail;
    }

    head_    = first_seg;
    cursegm_  = first_seg;
    segmline_ = 0;
    line_     = 0;
}

//
// Read line content from segment chain at specified index.
//
std::string Workspace::read_line_from_segment(int line_no)
{
    if (position(line_no))
        return "";

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
// When the needed line is beyond the end of file, creates empty segments.
// Returns 0 on success, 1 if line had to be created.
//
int Workspace::breaksegm(int line_no, bool realloc_flag)
{
    if (!head_ || !cursegm_)
        return 1;

    // Position workspace to the target line
    if (position(line_no)) {
        // Line is beyond end of file - create blank lines to extend it
        // This matches the prototype behavior
        int num_blank_lines = line_no - line_;
        if (num_blank_lines <= 0)
            return 1;

        Segment *blank_seg = create_blank_lines(num_blank_lines);
        if (!blank_seg)
            return 1;

        // Get the tail segment (the one we're currently at, beyond EOF)
        Segment *tail = cursegm_;
        Segment *prev = tail ? tail->prev : nullptr;

        // Remove tail temporarily
        if (prev)
            prev->next = nullptr;

        // Link blank segments before tail
        blank_seg->prev     = prev;
        Segment *last_blank = blank_seg;
        while (last_blank->next)
            last_blank = last_blank->next;
        last_blank->next = tail;
        tail->prev       = last_blank;

        // Update workspace
        if (prev) {
            prev->next = blank_seg;
        } else {
            set_chain(blank_seg);
        }

        // Update nlines to include the blank lines
        nlines_ = line_no + 1;

        // Position to the target line
        if (position(line_no))
            return 1;

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
            // Requested line beyond this blank segment
            return 1;
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
    // They must be from the same file and together have < 127 lines
    if (prev->fdesc == curr->fdesc && (prev->nlines + curr->nlines) < 127) {
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

    // Break at line 'to' (not to+1) to get the segment containing line 'to'
    // After this, cursegm_ points to the segment containing line 'to'
    int result = breaksegm(to, true);
    if (result != 0) {
        // Line doesn't exist - allow deletion to last line
        if (to + 1 > nlines_) {
            // Just move to the last line position
            position(nlines_);
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

    // Add a tail segment to the deleted chain
    end_seg->next        = new Segment();
    end_seg->next->prev  = end_seg;
    end_seg->next->fdesc = 0;

    // Count lines in deleted segments
    int deleted_lines = 0;
    Segment *del_seg  = start_seg;
    while (del_seg && del_seg != end_seg->next) {
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

    // Add tail segment
    if (copied_first) {
        Segment *tail     = new Segment();
        tail->nlines      = 0;
        tail->fdesc       = 0;
        tail->seek        = end ? end->seek : (curr ? curr->seek : 0);
        tail->next        = nullptr;
        tail->prev        = copied_last;
        copied_last->next = tail;
    }

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

    // Add tail segment
    if (first) {
        Segment *tail = new Segment();
        tail->nlines  = 0;
        tail->fdesc   = 0;
        tail->prev    = last;
        last->next    = tail;
    }

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
    position(line_);
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
