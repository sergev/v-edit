#include "segment.h"

#include <iostream>

//
// Calculate total bytes represented by all line lengths in this segment.
//
long Segment::get_total_bytes() const
{
    long total_bytes = 0;

    for (int i = 0; i < nlines; ++i) {
        total_bytes += sizes[i];
    }
    return total_bytes;
}

//
// Debug routine: print all fields in consistent format as single line.
//
void Segment::debug_print(std::ostream &out) const
{
    out << "Segment["
        << "prev=" << static_cast<void *>(prev) << ", "
        << "next=" << static_cast<void *>(next) << ", "
        << "nlines=" << nlines << ", "
        << "fdesc=" << fdesc << ", "
        << "seek=" << seek << ", "
        << "sizes={";

    for (int i = 0; i < nlines; ++i) {
        if (i > 0)
            out << ",";
        out << sizes[i];
    }

    out << "}]";
}
