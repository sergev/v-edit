#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <string>

// Class to encapsulate editor parameter system
class Parameters {
public:
    // Parameter types
    static constexpr int PARAM_NONE     = 0;
    static constexpr int PARAM_STRING   = 1;
    static constexpr int PARAM_AREA     = -1;
    static constexpr int PARAM_TAG_AREA = -2;

    Parameters() = default;

    // Getters
    int get_type() const { return type_; }
    const std::string &get_string() const { return str_; }
    int get_count() const { return count_; }

    // Area coordinates
    void get_area_start(int &col, int &row) const
    {
        col = c0_;
        row = r0_;
    }

    void get_area_end(int &col, int &row) const
    {
        col = c1_;
        row = r1_;
    }

    // Setters
    void set_type(int type) { type_ = type; }
    void set_string(const std::string &str) { str_ = str; }
    void set_count(int count) { count_ = count; }

    void set_area_start(int col, int row)
    {
        c0_ = col;
        r0_ = row;
    }

    void set_area_end(int col, int row)
    {
        c1_ = col;
        r1_ = row;
    }

    // Utility functions
    void reset()
    {
        type_ = PARAM_NONE;
        str_.clear();
        count_ = 0;
        c0_ = r0_ = c1_ = r1_ = 0;
    }

    // Area operations
    void normalize_area()
    {
        if (r0_ > r1_) {
            std::swap(r0_, r1_);
        }
        if (c0_ > c1_) {
            std::swap(c0_, c1_);
        }
    }

    bool is_horizontal_area() const { return r1_ == r0_; }
    bool is_vertical_area() const { return c1_ == c0_; }

private:
    int type_{ PARAM_NONE }; // Parameter type
    std::string str_;        // String parameter
    int c0_{ 0 }, r0_{ 0 };  // Area top-left corner
    int c1_{ 0 }, r1_{ 0 };  // Area bottom-right corner
    int count_{ 0 };         // Numeric count parameter
};

#endif // PARAMETERS_H
