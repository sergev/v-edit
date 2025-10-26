#include "segment.h"

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
