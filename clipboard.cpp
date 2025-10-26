#include "clipboard.h"

#include <iostream>

Clipboard::Clipboard()
    : start_line(-1), end_line(-1), start_col(-1), end_col(-1), m_is_rectangular(false)
{
}

bool Clipboard::is_empty() const
{
    return lines.empty();
}

bool Clipboard::is_rectangular() const
{
    return m_is_rectangular;
}

int Clipboard::get_start_line() const
{
    return start_line;
}

int Clipboard::get_end_line() const
{
    return end_line;
}

int Clipboard::get_start_col() const
{
    return start_col;
}

int Clipboard::get_end_col() const
{
    return end_col;
}

const std::vector<std::string> &Clipboard::get_lines() const
{
    return lines;
}

void Clipboard::clear()
{
    lines.clear();
    start_line = end_line = start_col = end_col = -1;
    m_is_rectangular                            = false;
}

void Clipboard::copy_lines(const std::vector<std::string> &source, int startLine, int count)
{
    clear();
    m_is_rectangular = false;
    start_line       = startLine;
    end_line         = startLine + count - 1;

    for (int i = 0; i < count; ++i) {
        if (startLine + i < (int)source.size()) {
            lines.push_back(source[startLine + i]);
        }
    }
}

void Clipboard::copy_rectangular_block(const std::vector<std::string> &source, int line, int col,
                                       int width, int height)
{
    clear();
    m_is_rectangular = true;
    start_line       = line;
    end_line         = line + height - 1;
    start_col        = col;
    end_col          = col + width - 1;

    for (int i = 0; i < height; ++i) {
        if (line + i < (int)source.size()) {
            const std::string &ln = source[line + i];
            std::string block_line;
            for (int j = 0; j < width; ++j) {
                if (col + j < (int)ln.size()) {
                    block_line += ln[col + j];
                } else {
                    block_line += ' '; // pad with spaces
                }
            }
            lines.push_back(block_line);
        } else {
            lines.push_back(std::string(width, ' '));
        }
    }
}

Clipboard::BlockData Clipboard::get_data() const
{
    BlockData data;
    data.lines          = lines;
    data.start_line     = start_line;
    data.end_line       = end_line;
    data.start_col      = start_col;
    data.end_col        = end_col;
    data.is_rectangular = m_is_rectangular;
    return data;
}

void Clipboard::set_data(bool rect, int s_line, int e_line, int s_col, int e_col,
                         const std::vector<std::string> &clipboard_lines)
{
    m_is_rectangular = rect;
    start_line       = s_line;
    end_line         = e_line;
    start_col        = s_col;
    end_col          = e_col;
    lines            = clipboard_lines;
}

void Clipboard::paste_into_lines(std::vector<std::string> &target, int afterLine)
{
    if (is_empty()) {
        return;
    }

    // Paste as lines
    target.insert(target.begin() + std::min((int)target.size(), afterLine + 1), lines.begin(),
                  lines.end());
}

void Clipboard::paste_into_rectangular(std::vector<std::string> &target, int afterLine, int atCol)
{
    if (is_empty()) {
        return;
    }

    int startLine = afterLine + 1;
    int numLines  = lines.size();
    int numCols   = end_col - start_col + 1;

    // Ensure we have enough lines
    while ((int)target.size() <= startLine + numLines - 1) {
        target.push_back("");
    }

    // Insert the rectangular block
    for (int i = 0; i < numLines; ++i) {
        std::string &targetLine = target[startLine + i];

        // Expand line if necessary
        if ((int)targetLine.size() < atCol + numCols) {
            targetLine.resize(atCol + numCols, ' ');
        }

        // Copy the block content
        if (i < (int)lines.size()) {
            const std::string &src = lines[i];
            for (int j = 0; j < numCols && j < (int)src.size(); ++j) {
                targetLine[atCol + j] = src[j];
            }
        }
    }
}

void Clipboard::serialize(std::ostream &out) const
{
    out << m_is_rectangular << '\n';
    out << start_line << '\n';
    out << end_line << '\n';
    out << start_col << '\n';
    out << end_col << '\n';
    out << lines.size() << '\n';
    for (const std::string &line : lines) {
        out << line << '\n';
    }
}

void Clipboard::deserialize(std::istream &in)
{
    in >> m_is_rectangular >> start_line >> end_line;
    in >> start_col >> end_col;
    size_t clip_count;
    in >> clip_count;
    lines.clear();
    for (size_t i = 0; i < clip_count; ++i) {
        std::string line;
        std::getline(in, line); // consume newline
        std::getline(in, line);
        lines.push_back(line);
    }
}
