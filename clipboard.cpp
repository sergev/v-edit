#include "clipboard.h"

#include <iostream>

Clipboard::Clipboard()
    : start_line_(-1), end_line_(-1), start_col_(-1), end_col_(-1), m_is_rectangular_(false)
{
}

bool Clipboard::is_empty() const
{
    return lines_.empty();
}

bool Clipboard::is_rectangular() const
{
    return m_is_rectangular_;
}

int Clipboard::get_start_line() const
{
    return start_line_;
}

int Clipboard::get_end_line() const
{
    return end_line_;
}

int Clipboard::get_start_col() const
{
    return start_col_;
}

int Clipboard::get_end_col() const
{
    return end_col_;
}

const std::vector<std::string> &Clipboard::get_lines() const
{
    return lines_;
}

void Clipboard::clear()
{
    lines_.clear();
    start_line_ = end_line_ = start_col_ = end_col_ = -1;
    m_is_rectangular_                               = false;
}

void Clipboard::copy_lines(const std::vector<std::string> &source, int startLine, int count)
{
    clear();
    m_is_rectangular_ = false;
    start_line_       = startLine;
    end_line_         = startLine + count - 1;

    for (int i = 0; i < count; ++i) {
        if (startLine + i < (int)source.size()) {
            lines_.push_back(source[startLine + i]);
        }
    }
}

void Clipboard::copy_rectangular_block(const std::vector<std::string> &source, int line, int col,
                                       int width, int height)
{
    clear();
    m_is_rectangular_ = true;
    start_line_       = line;
    end_line_         = line + height - 1;
    start_col_        = col;
    end_col_          = col + width - 1;

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
            lines_.push_back(block_line);
        } else {
            lines_.push_back(std::string(width, ' '));
        }
    }
}

Clipboard::BlockData Clipboard::get_data() const
{
    BlockData data;
    data.lines          = lines_;
    data.start_line     = start_line_;
    data.end_line       = end_line_;
    data.start_col      = start_col_;
    data.end_col        = end_col_;
    data.is_rectangular = m_is_rectangular_;
    return data;
}

void Clipboard::set_data(bool rect, int s_line, int e_line, int s_col, int e_col,
                         const std::vector<std::string> &clipboard_lines_)
{
    m_is_rectangular_ = rect;
    start_line_       = s_line;
    end_line_         = e_line;
    start_col_        = s_col;
    end_col_          = e_col;
    lines_            = clipboard_lines_;
}

void Clipboard::paste_into_lines(std::vector<std::string> &target, int afterLine)
{
    if (is_empty()) {
        return;
    }

    // Paste as lines_
    target.insert(target.begin() + std::min((int)target.size(), afterLine + 1), lines_.begin(),
                  lines_.end());
}

void Clipboard::paste_into_rectangular(std::vector<std::string> &target, int afterLine, int atCol)
{
    if (is_empty()) {
        return;
    }

    int startLine = afterLine + 1;
    int numLines  = lines_.size();
    int numCols   = end_col_ - start_col_ + 1;

    // Ensure we have enough lines_
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
        if (i < (int)lines_.size()) {
            const std::string &src = lines_[i];
            for (int j = 0; j < numCols && j < (int)src.size(); ++j) {
                targetLine[atCol + j] = src[j];
            }
        }
    }
}

void Clipboard::serialize(std::ostream &out) const
{
    out << m_is_rectangular_ << '\n';
    out << start_line_ << '\n';
    out << end_line_ << '\n';
    out << start_col_ << '\n';
    out << end_col_ << '\n';
    out << lines_.size() << '\n';
    for (const std::string &line : lines_) {
        out << line << '\n';
    }
}

void Clipboard::deserialize(std::istream &in)
{
    in >> m_is_rectangular_ >> start_line_ >> end_line_;
    in >> start_col_ >> end_col_;
    size_t clip_count;
    in >> clip_count;
    lines_.clear();
    for (size_t i = 0; i < clip_count; ++i) {
        std::string line;
        std::getline(in, line); // consume newline
        std::getline(in, line);
        lines_.push_back(line);
    }
}
