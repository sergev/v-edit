#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstddef>
#include <list>
#include <ostream>
#include <vector>

class Segment {
public:
    using iterator       = std::list<Segment>::iterator;
    using const_iterator = std::list<Segment>::const_iterator;

    // Each segment contains a non-zero number of text lines.
    int nlines{ 0 };

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
    int fdesc{ 0 };

    // Offset in fdesc for data of this segment.
    long seek{ 0 };

    // Line lengths, including "\n".
    std::vector<unsigned short> sizes;

    // Constructor.
    Segment() = default;

    // Calculate total bytes represented by all line lengths in this segment.
    long get_total_bytes() const;

    // Segment has contents when it comes from some file or contains only newlines.
    // A linked list of segments terminates with a placeholder with fdesc=0.
    bool has_contents() const { return fdesc != 0; }

    // Debug routine: print all fields in consistent format as single line.
    void debug_print(std::ostream &out) const;

private:
};

#endif // SEGMENT_H
