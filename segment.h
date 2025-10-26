#ifndef SEGMENT_H
#define SEGMENT_H

#include <cstddef>
#include <vector>

class Segment {
public:
    Segment *prev{ nullptr };
    Segment *next{ nullptr };
    int nlines{ 0 };
    int fdesc{ 0 };
    long seek{ 0 };
    std::vector<unsigned char> data;

    // Constructor
    Segment() = default;

    // Decode line length from segment data array at given index
    // Returns length including "\n"
    // Updates idx to point to next entry
    int decode_line_len(size_t &idx) const;

    // Add line length to segment data in encoded format
    // Handles both single-byte (length <= 127) and two-byte (length > 127) encoding
    void add_line_length(int line_len);

    // Calculate total bytes represented by all line lengths in this segment
    long get_total_bytes() const;

private:
};

#endif // SEGMENT_H
