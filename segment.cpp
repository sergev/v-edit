#include "segment.h"

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
