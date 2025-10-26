#include "workspace.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <fstream>

Workspace::Workspace() = default;

Workspace::~Workspace()
{
    cleanup_segments();
}

Workspace::Workspace(const Workspace &other)
{
    *this = other;
}

Workspace &Workspace::operator=(const Workspace &other)
{
    if (this == &other) {
        return *this;
    }

    // Clean up existing segments
    cleanup_segments();

    // Copy simple fields
    path_      = other.path_;
    writable_  = other.writable_;
    nlines_    = other.nlines_;
    topline_   = other.topline_;
    basecol_   = other.basecol_;
    line_      = other.line_;
    segmline_  = other.segmline_;
    cursorcol_ = other.cursorcol_;
    cursorrow_ = other.cursorrow_;

    // Copy segment chain (deep copy)
    if (other.chain_) {
        Segment *src  = other.chain_;
        Segment *prev = nullptr;

        while (src) {
            Segment *new_seg = new Segment();
            new_seg->prev    = prev;
            new_seg->next    = nullptr;
            new_seg->nlines  = src->nlines;
            new_seg->fdesc   = src->fdesc;
            new_seg->seek    = src->seek;
            new_seg->data    = src->data;

            if (prev) {
                prev->next = new_seg;
            } else {
                chain_ = new_seg;
            }

            if (src == other.cursegm_) {
                cursegm_ = new_seg;
            }

            prev = new_seg;
            src  = src->next;
        }
    }

    return *this;
}

void Workspace::cleanup_segments()
{
    if (chain_) {
        Segment *seg = chain_;
        while (seg) {
            Segment *next = seg->next;
            delete seg;
            seg = next;
        }
        chain_   = nullptr;
        cursegm_ = nullptr;
    }
}

void Workspace::reset()
{
    cleanup_segments();
    path_.clear();
    writable_  = 0;
    nlines_    = 0;
    topline_   = 0;
    basecol_   = 0;
    line_      = 0;
    segmline_  = 0;
    cursorcol_ = 0;
    cursorrow_ = 0;
}

//
// Build segment chain from in-memory lines vector.
//
void Workspace::build_segment_chain_from_lines(const std::vector<std::string> &lines)
{
    writable_ = 1;
    nlines_   = (int)lines.size();

    // Clean up old chain if any
    cleanup_segments();

    Segment *seg = new Segment();
    seg->prev    = nullptr;
    seg->next    = nullptr;
    seg->nlines  = nlines_;
    seg->fdesc   = 0;
    seg->seek    = 0;

    // Build segment chain with line length data
    seg->data.reserve(lines.size() * 2);
    for (const std::string &ln : lines) {
        int n = (int)ln.size() + 1; // include newline
        seg->add_line_length(n);
    }
    chain_ = seg;

    // Set workspace pointer to first segment
    cursegm_  = seg;
    segmline_ = 0;

    // Don't reset topline here as it's called during editing and would reset scroll position
    // Only reset offset and line
    basecol_ = 0;
    line_    = 0;
}

//
// Parse text and build segment chain from it.
//
void Workspace::build_segment_chain_from_text(const std::string &text)
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
    build_segment_chain_from_lines(lines_vec);
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
    while (lno >= segStart + seg->nlines && seg->fdesc) {
        segStart += seg->nlines;
        seg = seg->next;
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
    size_t idx   = 0;
    for (int i = 0; i < rel; ++i) {
        int len = seg->decode_line_len(idx);
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
    path_ = path;

    // Open file for reading
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return;
    }

    // Build segment chain from file
    build_segment_chain_from_file(fd);

    close(fd);
}

//
// Build segment chain from file descriptor.
//
void Workspace::build_segment_chain_from_file(int fd)
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
                    seg->data    = temp_seg.data;

                    // Reset temp segment for next use
                    temp_seg.data.clear();

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

        temp_seg.add_line_length(line_len);

        ++lines_in_seg;
        file_offset += line_len;

        // Create new segment if we've hit limits
        if (lines_in_seg >= 127 || temp_seg.data.size() >= 4000) {
            Segment *seg = new Segment();
            seg->prev    = last_seg;
            seg->next    = nullptr;
            seg->nlines  = lines_in_seg;
            seg->fdesc   = fd;
            seg->seek    = seg_seek;
            seg->data    = temp_seg.data;

            if (last_seg)
                last_seg->next = seg;
            else
                first_seg = seg;

            last_seg = seg;
            nlines_ += lines_in_seg;

            // Reset for next segment
            temp_seg.data.clear();
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

    chain_    = first_seg;
    cursegm_  = first_seg;
    segmline_ = 0;
    line_     = 0;
}

//
// Read line content from segment chain at specified index.
//
std::string Workspace::read_line_from_segment(int line_no)
{
    if (path_.empty())
        return std::string();

    if (position(line_no))
        return std::string();

    Segment *seg = cursegm_;
    int rel_line = line_no - segmline_;

    // Calculate file offset by accumulating line lengths
    // Note: seg->seek points to the START of the first line in the segment
    // We need to skip 'rel_line' lines to get to the line we want
    long seek_pos = seg->seek;
    size_t idx    = 0;
    for (int i = 0; i < rel_line; ++i) {
        int len = seg->decode_line_len(idx);
        seek_pos += len;
    }

    // Get line length
    int line_len = seg->decode_line_len(idx);
    if (line_len <= 0)
        return std::string();

    // Read line from file
    char *buffer = new char[line_len];

    // Open file and read
    int fd = open(path_.c_str(), O_RDONLY);
    if (fd >= 0) {
        if (lseek(fd, seek_pos, SEEK_SET) >= 0) {
            read(fd, buffer, line_len);
        }
        close(fd);
    }

    std::string result(buffer, line_len - 1); // exclude newline
    delete[] buffer;

    return result;
}

//
// Write segment chain content to file.
//
bool Workspace::write_segments_to_file(const std::string &path)
{
    if (!chain_ || path_.empty())
        return false;

    int out_fd = creat(path.c_str(), 0664);
    if (out_fd < 0)
        return false;

    // Open source file
    int in_fd = open(path_.c_str(), O_RDONLY);
    if (in_fd < 0) {
        close(out_fd);
        return false;
    }

    Segment *seg = chain_;
    char buffer[8192];

    while (seg && seg->fdesc) {
        if (seg->fdesc > 0) {
            // Calculate total bytes for this segment
            long total_bytes = seg->get_total_bytes();

            // Read from source file and write to output
            lseek(in_fd, seg->seek, SEEK_SET);
            while (total_bytes > 0) {
                int to_read = (total_bytes < (long)sizeof(buffer)) ? total_bytes : sizeof(buffer);
                int nread   = read(in_fd, buffer, to_read);
                if (nread <= 0)
                    break;
                write(out_fd, buffer, nread);
                total_bytes -= nread;
            }
        }
        seg = seg->next;
    }

    close(in_fd);
    close(out_fd);
    return true;
}

//
// Check if workspace is using file-based segments.
//
bool Workspace::is_file_based() const
{
    if (!chain_) {
        return false;
    }
    Segment *seg = chain_;
    return seg && seg->fdesc > 0;
}

//
// Get total line count. Returns workspace line count if segments exist,
// otherwise returns the fallback count from the lines vector.
//
int Workspace::get_line_count(int fallback_count) const
{
    return chain_ ? nlines_ : fallback_count;
}

//
// Split segment at given line number (based on breaksegm from prototype).
// When the needed line is beyond the end of file, creates empty segments.
// Returns 0 on success, 1 if line is beyond end of file.
//
int Workspace::breaksegm(int line_no, bool realloc_flag)
{
    if (!chain_ || !cursegm_)
        return 1;

    // Position workspace to the target line
    if (position(line_no))
        return 1;

    // Now we're at the segment containing line_no
    Segment *seg = cursegm_;
    int rel_line = line_no - segmline_;

    if (rel_line == 0) {
        return 0; // Already at the right position
    }

    // Calculate file offset and find split point by walking through lines
    size_t data_idx = 0;
    long offs       = 0;

    // Walk through the first rel_line lines to calculate offset
    for (int i = 0; i < rel_line; ++i) {
        if (data_idx >= seg->data.size())
            break;

        int len = seg->decode_line_len(data_idx);
        offs += len;
    }

    // Record where we are in the data after processing rel_line entries
    size_t split_point = data_idx;

    // Create new segment for the remainder
    Segment *new_seg = new Segment();
    new_seg->nlines  = seg->nlines - rel_line;
    new_seg->fdesc   = seg->fdesc;
    new_seg->seek    = seg->seek + offs;

    // Copy remaining data bytes from split_point to end
    for (size_t i = split_point; i < seg->data.size(); ++i) {
        new_seg->data.push_back(seg->data[i]);
    }

    // Link new segment
    new_seg->next = seg->next;
    new_seg->prev = seg;
    if (seg->next)
        seg->next->prev = new_seg;
    seg->next = new_seg;

    // Truncate original segment data - keep only first rel_line data
    std::vector<unsigned char> new_data;
    for (size_t i = 0; i < split_point; ++i) {
        new_data.push_back(seg->data[i]);
    }
    seg->data   = new_data;
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
    if (!chain_ || !cursegm_ || !cursegm_->prev)
        return false;

    Segment *curr = cursegm_;
    Segment *prev = curr->prev;

    // Check if segments can be merged
    // They must be from the same file and together have < 127 lines
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
            merged->data = prev->data;
            for (unsigned char byte : curr->data)
                merged->data.push_back(byte);

            // Update links
            if (prev->prev)
                prev->prev->next = merged;
            else
                chain_ = merged;

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
    Segment *last = new_seg;
    while (last->next && last->next->fdesc != 0)
        last = last->next;

    // Split at insertion point
    if (breaksegm(at, true) == 0) {
        // Link new segments in
        Segment *after = cursegm_->next;
        cursegm_->next = new_seg;
        new_seg->prev  = cursegm_;

        if (after)
            after->prev = last;

        // Update workspace position
        cursegm_  = new_seg;
        segmline_ = at;
    }

    writable_ = 1; // Mark as edited
}

//
// Delete segments between from and to lines (based on delete from prototype).
// Returns the deleted segment chain.
//
Segment *Workspace::delete_segments(int from, int to)
{
    if (!chain_ || from > to)
        return nullptr;

    // Break at end line + 1
    if (breaksegm(to + 1, true) != 0)
        return nullptr;

    Segment *end_seg = cursegm_;
    Segment *after   = end_seg->next; // Save pointer to what comes after

    // Break at start line
    if (breaksegm(from, true) != 0)
        return nullptr;

    Segment *start_seg = cursegm_;
    Segment *before    = start_seg->prev;

    // Unlink the segment chain to delete
    if (before)
        before->next = after;
    else
        chain_ = after;

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
        copy->data    = curr->data; // Copy vector
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
        seg->data.clear();

        // Add line length data: each empty line has length 1 (just newline)
        for (int i = 0; i < lines_in_seg; ++i) {
            seg->add_line_length(1);
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
