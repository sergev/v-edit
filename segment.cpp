#include "segment.h"

//
// Decode line length from segment data array.
//
int Segment::decode_line_len(size_t &idx) const
{
    if (idx >= data.size())
        return 0;
    unsigned char b = data[idx++];
    if (b & 0200) {
        // two-byte length
        if (idx >= data.size())
            return 0;
        unsigned char b2 = data[idx++];
        return (int)((b & 0177) * 128 + b2);
    }
    return (int)b;
}

//
// Add line length to segment data in encoded format.
//
void Segment::add_line_length(int line_len)
{
    if (line_len > 127) {
        data.push_back((unsigned char)((line_len / 128) | 0200));
        data.push_back((unsigned char)(line_len % 128));
    } else {
        data.push_back((unsigned char)line_len);
    }
}

//
// Calculate total bytes represented by all line lengths in this segment.
//
long Segment::get_total_bytes() const
{
    long total_bytes = 0;
    size_t idx       = 0;
    for (int i = 0; i < nlines; ++i) {
        int len = decode_line_len(idx);
        total_bytes += len;
    }
    return total_bytes;
}
