#include "clipboard.h"

#include <iostream>

#include "editor.h"

// ============================================================================
// Clipboard class implementation
// ============================================================================

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

void Clipboard::copy_lines(const std::vector<std::string> &source, int start_line, int count)
{
    clear();
    m_is_rectangular_ = false;
    start_line_       = start_line;
    end_line_         = start_line + count - 1;

    for (int i = 0; i < count; ++i) {
        if (start_line + i < (int)source.size()) {
            lines_.push_back(source[start_line + i]);
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

void Clipboard::paste_into_lines(std::vector<std::string> &target, int after_line)
{
    if (is_empty()) {
        return;
    }

    // Paste as lines_
    target.insert(target.begin() + std::min((int)target.size(), after_line + 1), lines_.begin(),
                  lines_.end());
}

void Clipboard::paste_into_rectangular(std::vector<std::string> &target, int after_line, int at_col)
{
    if (is_empty()) {
        return;
    }

    int start_line = after_line + 1;
    int num_lines  = lines_.size();
    int num_cols   = end_col_ - start_col_ + 1;

    // Ensure we have enough lines_
    while ((int)target.size() <= start_line + num_lines - 1) {
        target.push_back("");
    }

    // Insert the rectangular block
    for (int i = 0; i < num_lines; ++i) {
        std::string &target_line = target[start_line + i];

        // Expand line if necessary
        if ((int)target_line.size() < at_col + num_cols) {
            target_line.resize(at_col + num_cols, ' ');
        }

        // Copy the block content
        if (i < (int)lines_.size()) {
            const std::string &src = lines_[i];
            for (int j = 0; j < num_cols && j < (int)src.size(); ++j) {
                target_line[at_col + j] = src[j];
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

// ============================================================================
// Editor clipboard operations
// ============================================================================

//
// Copy specified lines to clipboard_.
//
void Editor::picklines(int start_line, int count)
{
    if (count <= 0 || start_line < 0) {
        return;
    }

    put_line(); // Save any unsaved modifications

    // Read lines from workspace segments
    std::vector<std::string> lines;
    auto total = wksp_->total_line_count();
    for (int i = 0; i < count && (start_line + i) < total; ++i) {
        std::string line = wksp_->read_line(start_line + i);
        lines.push_back(line);
    }

    // Store in clipboard
    clipboard_.copy_lines(lines, 0, lines.size());
}

//
// Insert clipboard content at specified position.
//
void Editor::paste(int after_line, int at_col)
{
    if (clipboard_.is_empty()) {
        return;
    }

    put_line(); // Save any unsaved modifications

    const std::vector<std::string> &clip_lines = clipboard_.get_lines();

    if (clipboard_.is_rectangular()) {
        // Paste as rectangular block - insert at column position
        auto total = wksp_->total_line_count();
        for (size_t i = 0; i < clip_lines.size() && (after_line + (int)i) < total; ++i) {
            get_line(after_line + i);
            if (at_col < (int)current_line_.size()) {
                current_line_.insert(at_col, clip_lines[i]);
            } else {
                // Extend line with spaces if needed
                current_line_.resize(at_col, ' ');
                current_line_ += clip_lines[i];
            }
            current_line_modified_ = true;
            put_line();
        }
    } else {
        // Paste as lines
        auto clip_segments = tempfile_.write_lines_to_temp(clip_lines);

        // Insert the segments into workspace at the correct position
        wksp_->insert_contents(clip_segments, after_line);
    }

    ensure_cursor_visible();
}

//
// Copy rectangular block to clipboard_.
//
void Editor::pickspaces(int line, int col, int number, int nl)
{
    if (number <= 0 || nl <= 0 || line < 0 || col < 0) {
        return;
    }

    put_line(); // Save any unsaved modifications

    // Read lines from workspace and extract rectangular block
    std::vector<std::string> lines;
    auto total = wksp_->total_line_count();
    for (int i = 0; i < nl && (line + i) < total; ++i) {
        std::string full_line = wksp_->read_line(line + i);
        std::string block;

        if (col < (int)full_line.size()) {
            int end_col = std::min(col + number, (int)full_line.size());
            block       = full_line.substr(col, end_col - col);
        }

        lines.push_back(block);
    }

    // Store rectangular block in clipboard
    clipboard_.copy_rectangular_block(lines, 0, col, number, lines.size());
}

//
// Delete rectangular block and save to clipboard_.
//
void Editor::closespaces(int line, int col, int number, int nl)
{
    // Delete rectangular area and save to clipboard
    pickspaces(line, col, number, nl); // copy first

    // Now delete the rectangular area using get_line/put_line pattern
    put_line();
    auto total = wksp_->total_line_count();
    for (int i = 0; i < nl; ++i) {
        if (line + i < total) {
            get_line(line + i);
            if (col < (int)current_line_.size()) {
                int end_pos = std::min(col + number, (int)current_line_.size());
                current_line_.erase(col, end_pos - col);
                current_line_modified_ = true;
                put_line();
            }
        }
    }
    ensure_cursor_visible();
}

//
// Insert spaces into rectangular area.
//
void Editor::openspaces(int line, int col, int number, int nl)
{
    // Insert spaces in rectangular area using get_line/put_line pattern
    put_line();
    auto total = wksp_->total_line_count();
    for (int i = 0; i < nl; ++i) {
        if (line + i < total) {
            get_line(line + i);
            if (col <= (int)current_line_.size()) {
                current_line_.insert(col, number, ' ');
            } else {
                // Extend line with spaces if needed
                current_line_.resize(col, ' ');
                current_line_.insert(col, number, ' ');
            }
            current_line_modified_ = true;
            put_line();
        } else {
            // Create new line if needed
            auto blank = wksp_->create_blank_lines(1);
            wksp_->insert_contents(blank, line + i);
        }
    }
    ensure_cursor_visible();
}
