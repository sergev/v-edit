#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <string>
#include <algorithm>

// Parameter types
struct Parameters {
    static constexpr int PARAM_NONE     = 0;
    static constexpr int PARAM_STRING   = 1;
    static constexpr int PARAM_AREA     = -1;
    static constexpr int PARAM_TAG_AREA = -2;

    // Public fields
    int type{ PARAM_NONE }; // Parameter type
    std::string str;        // String parameter
    int c0{ 0 }, r0{ 0 };   // Area top-left corner
    int c1{ 0 }, r1{ 0 };   // Area bottom-right corner
    int count{ 0 };         // Numeric count parameter

    // Utility functions
    void reset()
    {
        type = PARAM_NONE;
        str.clear();
        count = 0;
        c0 = r0 = c1 = r1 = 0;
    }

    // Area operations
    void normalize_area()
    {
        if (r0 > r1) {
            std::swap(r0, r1);
        }
        if (c0 > c1) {
            std::swap(c0, c1);
        }
    }

    // On input, rA and cA are coordinates of one corner of the area.
    // On output (rB and cB), return coordinates of the opposite corner.
    void get_opposite_corner(int rA, int cA, int &rB, int &cB)
    {
        rB = (rA == r0) ? r1 : r0;
        cB = (cA == c0) ? c1 : c0;
    }

    bool is_horizontal_area() const { return r1 == r0; }
    bool is_vertical_area() const { return c1 == c0; }
};

#endif // PARAMETERS_H
