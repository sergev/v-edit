#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstddef>
#include <vector>

class Segment {
public:
    // Segments a linked in a list.
    Segment *prev{ nullptr };
    Segment *next{ nullptr };

    // Each segment contains a non-zero number of text lines.
    int nlines{ 0 };

    // Descriptor of the file, where these text lines are stored.
    // There are three cases:
    //  * fdesc == original_fd of the enclosing workspace
    //      - when this segment holds unmodified lines of the original file.
    //  * fdesc == tempfile_fd
    //      - when this segment holds modified lines stored in temporary file.
    //  * fdesc == -1
    //      - when this segment holds empty lines (only newlines).
    // Note: fdesc cannot be 0 in a valid segment.
    int fdesc{ 0 };

    // Offset in fdesc for data of this segment.
    long seek{ 0 };

    // Line lengths, including "\n"
    std::vector<unsigned short> sizes;

    // Constructor
    Segment() = default;

    // Calculate total bytes represented by all line lengths in this segment
    long get_total_bytes() const;

private:
};

#endif // SEGMENT_H
