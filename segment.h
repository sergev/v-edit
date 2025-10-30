#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstddef>
#include <list>
#include <ostream>
#include <vector>

class Segment {
public:
    // Use iterators instead of pointers.
    using iterator = std::list<Segment>::iterator;

    // Each segment contains a non-zero number of text lines.
    unsigned line_count{ 0 };

    // Descriptor of the file, where these text lines are stored.
    // Cases:
    //  * file_descriptor == original_fd of the enclosing workspace
    //      - this segment holds unmodified lines of the original file.
    //  * file_descriptor == tempfile_fd
    //      - this segment holds modified lines stored in temporary file.
    //  * file_descriptor == -1
    //      - this segment holds empty lines (only newlines).
    int file_descriptor{ -1 };

    // Offset in file_descriptor for data of this segment.
    long file_offset{ 0 };

    // Line lengths, including "\n".
    std::vector<unsigned short> line_lengths;

    // Constructor.
    Segment() = default;

    // Segment has contents when it comes from some file or contains only newlines.

    // Calculate total bytes represented by all line lengths in this segment.
    long total_byte_count() const;

    // Debug routine: print all fields in consistent format as single line.
    void debug_print(std::ostream &out) const;

private:
};

#endif // SEGMENT_H
