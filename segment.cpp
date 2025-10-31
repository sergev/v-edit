#include "segment.h"

#include <unistd.h>

#include <iostream>
#include <utility>

//
// Constructor with parameters.
//
Segment::Segment(int file_descriptor_, unsigned line_count_, long file_offset_,
                 std::vector<unsigned short> &&line_lengths_)
    : line_count(line_count_), file_descriptor(file_descriptor_), file_offset(file_offset_),
      line_lengths(std::move(line_lengths_))
{
    if (file_descriptor < 0 && line_count > 0 && line_lengths.empty()) {
        // Empty lines: allocate missing lengths.
        line_lengths.resize(line_count, 1);
    }
}

//
// Calculate total bytes represented by all line lengths in this segment.
//
long Segment::total_byte_count() const
{
    long total_bytes = 0;

    for (int i = 0; i < line_count; ++i) {
        total_bytes += line_lengths[i];
    }
    return total_bytes;
}

//
// Calculate the file offset for a given relative line index within this segment.
// Returns the offset where the specified line begins in the file.
//
long Segment::calculate_line_offset(int rel_line) const
{
    long seek_pos = file_offset;
    for (int i = 0; i < rel_line; ++i) {
        seek_pos += line_lengths[i];
    }
    return seek_pos;
}

//
// Read line content from file at the specified relative line index.
// Returns empty string for empty lines, blank segments, or read errors.
//
std::string Segment::read_line_content(int rel_line) const
{
    // Validate relative line is within bounds
    if (rel_line < 0 || rel_line >= static_cast<int>(line_lengths.size())) {
        return "";
    }

    // Get line length
    int line_len = line_lengths[rel_line];
    if (line_len <= 0) {
        return "";
    }

    // Handle empty lines and blank segments
    if (line_len == 1 || file_descriptor < 0) {
        return "";
    }

    // Calculate file offset for this line
    long seek_pos = calculate_line_offset(rel_line);

    // Read line content from file (excluding newline)
    std::string result(line_len - 1, '\0');
    if (result.size() > 0 && lseek(file_descriptor, seek_pos, SEEK_SET) >= 0) {
        read(file_descriptor, &result[0], result.size());
    }
    return result;
}

//
// Write segment content to output file descriptor.
// Returns true on success, false on error (seek or write failure).
//
bool Segment::write_content(int out_fd) const
{
    // Calculate total bytes for this segment
    long total_bytes = total_byte_count();

    if (file_descriptor > 0) {
        // Read from source file and write to output
        if (lseek(file_descriptor, file_offset, SEEK_SET) < 0) {
            // Failed to seek - file may have been unlinked
            return false;
        }

        char buffer[8192];
        while (total_bytes > 0) {
            int to_read = (total_bytes < (long)sizeof(buffer)) ? total_bytes : sizeof(buffer);
            int nread   = read(file_descriptor, buffer, to_read);
            if (nread <= 0) {
                break;
            }

            write(out_fd, buffer, nread);
            total_bytes -= nread;
        }
    } else {
        // Empty lines - write newlines
        std::string newlines(total_bytes, '\n');
        write(out_fd, newlines.data(), newlines.size());
    }
    return true;
}

//
// Check if this segment can be merged with another segment.
//
bool Segment::can_merge_with(const Segment &other) const
{
    // Segments must be from the same file, and together have < 127 lines
    return file_descriptor > 0 && file_descriptor == other.file_descriptor &&
           (line_count + other.line_count) < 127;
}

//
// Check if this segment is adjacent to another segment.
//
bool Segment::is_adjacent_to(const Segment &other) const
{
    long prev_bytes = total_byte_count();
    return other.file_offset == file_offset + prev_bytes;
}

//
// Merge another segment into this segment.
//
void Segment::merge_with(const Segment &other)
{
    // Combine data into this segment
    for (unsigned short byte : other.line_lengths) {
        line_lengths.push_back(byte);
    }
    line_count += other.line_count;
}

//
// Debug routine: print all fields in consistent format as single line.
//
void Segment::debug_print(std::ostream &out) const
{
    out << "Segment "
        << "line_count=" << line_count << ", "
        << "file_descriptor=" << file_descriptor << ", "
        << "file_offset=" << file_offset << ", "
        << "line_lengths={";

    for (int i = 0; i < line_count; ++i) {
        if (i > 0)
            out << ",";
        out << line_lengths[i];
    }

    out << "}\n";
}
