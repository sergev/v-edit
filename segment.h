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
    int line_count{ 0 };

    // Descriptor of the file, where these text lines are stored.
    // There are four cases:
    //  * file_descriptor == original_fd of the enclosing workspace
    //      - when this segment holds unmodified lines of the original file.
    //  * file_descriptor == tempfile_fd
    //      - when this segment holds modified lines stored in temporary file.
    //  * file_descriptor == -1
    //      - when this segment holds empty lines (only newlines).
    //  * file_descriptor == 0
    //      - placeholder without contents (tail).
    int file_descriptor{ 0 };

    // Offset in file_descriptor for data of this segment.
    long file_offset{ 0 };

    // Line lengths, including "\n".
    std::vector<unsigned short> line_lengths;

    // Constructor.
    Segment() = default;

    // Segment has contents when it comes from some file or contains only newlines.
    // A linked list of segments terminates with a placeholder with file_descriptor=0.
    bool has_contents() const
    {
        return file_descriptor != 0;
    } // TODO: rename as is_empty(), invert meaning

    // Calculate total bytes represented by all line lengths in this segment.
    long get_total_bytes() const; // TODO: rename as total_byte_count()

    // Debug routine: print all fields in consistent format as single line.
    void debug_print(std::ostream &out) const;

private:
};

#endif // SEGMENT_H
