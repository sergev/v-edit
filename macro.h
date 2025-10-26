#ifndef MACRO_H
#define MACRO_H

#include <iostream>
#include <string>
#include <utility>
#include <vector>

class Macro {
public:
    enum Type { POSITION, BUFFER };

    Type type;
    std::pair<int, int> position;                 // for POSITION type
    std::vector<std::string> buffer_lines;        // for BUFFER type
    int start_line, end_line, start_col, end_col; // buffer bounds
    bool is_rectangular;

    Macro();

    // Check if this is a position marker
    bool isPosition() const;

    // Check if this is a buffer
    bool isBuffer() const;

    // Check if buffer is empty
    bool isBufferEmpty() const;

    // Set position data
    void setPosition(int line, int col);

    // Set buffer data
    void setBuffer(const std::vector<std::string> &lines, int s_line, int e_line, int s_col,
                   int e_col, bool is_rect);

    // Get position (returns 0,0 if not a position macro)
    std::pair<int, int> getPosition() const;

    // Get buffer bounds
    void getBufferBounds(int &s_line, int &e_line, int &s_col, int &e_col, bool &is_rect) const;

    // Get buffer lines (for BUFFER type)
    const std::vector<std::string> &getBufferLines() const;

    // Check if macro exists and has valid data
    bool isValid() const;

    // Get all buffer data at once (for restoring to clipboard)
    struct BufferData {
        std::vector<std::string> lines;
        int start_line, end_line;
        int start_col, end_col;
        bool is_rectangular;
    };
    BufferData getAllBufferData() const;

    // Serialization
    void serialize(std::ostream &out) const;
    void deserialize(std::istream &in);
};

#endif // MACRO_H
