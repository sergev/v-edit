#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>

#include "editor.h"
#include "segment.h"

//
// Load file content into segment chain structure.
//
void Editor::load_file_to_segments(const std::string &path)
{
    if (files.empty())
        model_init();

    FileDesc &f = files[wksp.wfile];
    f.path      = path;

    // Open file for reading
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        status = std::string("Cannot open file: ") + path;
        return;
    }

    // Build segment chain from file
    build_segment_chain_from_file(fd);

    close(fd);
}

//
// Build segment chain from file descriptor.
//
void Editor::build_segment_chain_from_file(int fd)
{
    if (files.empty())
        model_init();

    FileDesc &f = files[wksp.wfile];
    f.nlines    = 0;

    // Clean up old chain
    if (f.chain) {
        Segment *seg = f.chain;
        while (seg) {
            Segment *next = seg->next;
            delete seg;
            seg = next;
        }
        f.chain = nullptr;
    }

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
                    f.nlines += lines_in_seg;
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
            f.nlines += lines_in_seg;

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

    f.chain       = first_seg;
    wksp.cursegm  = first_seg;
    wksp.segmline = 0;
    wksp.line     = 0;
}

//
// Read line content from segment chain at specified index.
//
std::string Editor::read_line_from_segment(int line_no)
{
    if (files.empty() || files[wksp.wfile].path.empty())
        return std::string();

    if (wksp_position(line_no))
        return std::string();

    Segment *seg = wksp.cursegm;
    int rel_line = line_no - wksp.segmline;

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
    int fd = open(files[wksp.wfile].path.c_str(), O_RDONLY);
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
bool Editor::write_segments_to_file(const std::string &path)
{
    if (files.empty() || !files[wksp.wfile].chain || files[wksp.wfile].path.empty())
        return false;

    int out_fd = creat(path.c_str(), 0664);
    if (out_fd < 0)
        return false;

    // Open source file
    int in_fd = open(files[wksp.wfile].path.c_str(), O_RDONLY);
    if (in_fd < 0) {
        close(out_fd);
        return false;
    }

    Segment *seg = files[wksp.wfile].chain;
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
