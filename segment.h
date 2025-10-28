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
    int nlines{ 0 }; // TODO: rename as line_count

    // Descriptor of the file, where these text lines are stored.
    // There are four cases:
    //  * fdesc == original_fd of the enclosing workspace
    //      - when this segment holds unmodified lines of the original file.
    //  * fdesc == tempfile_fd
    //      - when this segment holds modified lines stored in temporary file.
    //  * fdesc == -1
    //      - when this segment holds empty lines (only newlines).
    //  * fdesc == 0
    //      - placeholder without contents (tail).
    int fdesc{ 0 }; // TODO: rename as file_descriptor

    // Offset in fdesc for data of this segment.
    long seek{ 0 }; // TODO: rename as file_offset

    // Line lengths, including "\n".
    std::vector<unsigned short> sizes; // TODO: rename as line_lengths

    // Constructor.
    Segment() = default;

    // Segment has contents when it comes from some file or contains only newlines.
    // A linked list of segments terminates with a placeholder with fdesc=0.
    bool has_contents() const { return fdesc != 0; } // TODO: rename as is_empty(), invert meaning

    // Calculate total bytes represented by all line lengths in this segment.
    long get_total_bytes() const; // TODO: rename as total_byte_count()

    // Debug routine: print all fields in consistent format as single line.
    void debug_print(std::ostream &out) const;

private:
};

#endif // SEGMENT_H
