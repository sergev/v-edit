#include "segment.h"

#include <iostream>

//
// Calculate total bytes represented by all line lengths in this segment.
//
long Segment::get_total_bytes() const
{
    long total_bytes = 0;

    for (int i = 0; i < line_count; ++i) {
        total_bytes += line_lengths[i];
    }
    return total_bytes;
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
