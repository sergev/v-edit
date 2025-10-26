#include "macro.h"

#include <iostream>

Macro::Macro()
    : type(POSITION), position({ 0, 0 }), start_line(-1), end_line(-1), start_col(-1), end_col(-1),
      is_rectangular(false)
{
}

bool Macro::isPosition() const
{
    return type == POSITION;
}

bool Macro::isBuffer() const
{
    return type == BUFFER;
}

bool Macro::isBufferEmpty() const
{
    return type == BUFFER && buffer_lines.empty();
}

void Macro::setPosition(int line, int col)
{
    type     = POSITION;
    position = std::make_pair(line, col);
    // Clear buffer data
    buffer_lines.clear();
    start_line = end_line = start_col = end_col = -1;
    is_rectangular                              = false;
}

void Macro::setBuffer(const std::vector<std::string> &lines, int s_line, int e_line, int s_col,
                      int e_col, bool is_rect)
{
    type           = BUFFER;
    buffer_lines   = lines;
    start_line     = s_line;
    end_line       = e_line;
    start_col      = s_col;
    end_col        = e_col;
    is_rectangular = is_rect;
}

std::pair<int, int> Macro::getPosition() const
{
    return type == POSITION ? position : std::make_pair(0, 0);
}

void Macro::getBufferBounds(int &s_line, int &e_line, int &s_col, int &e_col, bool &is_rect) const
{
    if (type == BUFFER) {
        s_line  = this->start_line;
        e_line  = this->end_line;
        s_col   = this->start_col;
        e_col   = this->end_col;
        is_rect = this->is_rectangular;
    } else {
        s_line = e_line = s_col = e_col = -1;
        is_rect                         = false;
    }
}

const std::vector<std::string> &Macro::getBufferLines() const
{
    return buffer_lines;
}

bool Macro::isValid() const
{
    if (type == POSITION) {
        return position.first >= 0 && position.second >= 0;
    } else if (type == BUFFER) {
        return !buffer_lines.empty();
    }
    return false;
}

Macro::BufferData Macro::getAllBufferData() const
{
    BufferData data;
    if (type == BUFFER) {
        data.lines          = buffer_lines;
        data.start_line     = start_line;
        data.end_line       = end_line;
        data.start_col      = start_col;
        data.end_col        = end_col;
        data.is_rectangular = is_rectangular;
    }
    return data;
}

void Macro::serialize(std::ostream &out) const
{
    out << (int)type << '\n';
    if (type == POSITION) {
        out << position.first << '\n';
        out << position.second << '\n';
    } else if (type == BUFFER) {
        out << start_line << '\n';
        out << end_line << '\n';
        out << start_col << '\n';
        out << end_col << '\n';
        out << is_rectangular << '\n';
        out << buffer_lines.size() << '\n';
        for (const std::string &line : buffer_lines) {
            out << line << '\n';
        }
    }
}

void Macro::deserialize(std::istream &in)
{
    int type_int;
    in >> type_int;
    type = (Type)type_int;

    if (type == POSITION) {
        int line, col;
        in >> line >> col;
        position = std::make_pair(line, col);
        // Clear buffer data
        buffer_lines.clear();
        start_line = end_line = start_col = end_col = -1;
        is_rectangular                              = false;
    } else if (type == BUFFER) {
        in >> start_line >> end_line >> start_col >> end_col;
        in >> is_rectangular;
        size_t line_count;
        in >> line_count;
        buffer_lines.clear();
        for (size_t i = 0; i < line_count; ++i) {
            std::string line;
            std::getline(in, line); // consume newline
            std::getline(in, line);
            buffer_lines.push_back(line);
        }
    }
}
