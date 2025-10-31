#include <iostream>

#include "editor.h"
#include "macro.h"

// ============================================================================
// Macro class implementation
// ============================================================================

Macro::Macro()
    : type(POSITION), position({ 0, 0 }), start_line(-1), end_line(-1), start_col(-1), end_col(-1),
      is_rectangular(false)
{
}

bool Macro::is_position() const
{
    return type == POSITION;
}

bool Macro::is_buffer() const
{
    return type == BUFFER;
}

bool Macro::is_buffer_empty() const
{
    return type == BUFFER && buffer_lines.empty();
}

void Macro::set_position(int line, int col)
{
    type     = POSITION;
    position = std::make_pair(line, col);
    // Clear buffer data
    buffer_lines.clear();
    start_line = end_line = start_col = end_col = -1;
    is_rectangular                              = false;
}

void Macro::set_buffer(const std::vector<std::string> &lines, int s_line, int e_line, int s_col,
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

std::pair<int, int> Macro::get_position() const
{
    return type == POSITION ? position : std::make_pair(0, 0);
}

void Macro::get_buffer_bounds(int &s_line, int &e_line, int &s_col, int &e_col, bool &is_rect) const
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

const std::vector<std::string> &Macro::get_buffer_lines() const
{
    return buffer_lines;
}

bool Macro::is_valid() const
{
    if (type == POSITION) {
        return position.first >= 0 && position.second >= 0;
    } else if (type == BUFFER) {
        return !buffer_lines.empty();
    }
    return false;
}

Macro::BufferData Macro::get_all_buffer_data() const
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

// ============================================================================
// Editor macro/buffer operations
// ============================================================================

//
// Save current cursor position to named macro.
//
void Editor::save_macro_position(char name)
{
    int abs_line = wksp_->view.topline + cursor_line_;
    int abs_col  = wksp_->view.basecol + cursor_col_;
    macros_[name].set_position(abs_line, abs_col);
}

//
// Navigate to position stored in named macro.
//
bool Editor::goto_macro_position(char name)
{
    auto it = macros_.find(name);
    if (it == macros_.end() || !it->second.is_position())
        return false;
    auto pos = it->second.get_position();
    goto_line(pos.first);
    wksp_->view.basecol = pos.second;
    cursor_col_         = 0;
    ensure_cursor_visible();
    return true;
}

//
// Save current clipboard to named buffer.
//
void Editor::save_macro_buffer(char name)
{
    // Save current clipboard content to named macro buffer
    auto data = clipboard_.get_data();
    macros_[name].set_buffer(data.lines, data.start_line, data.end_line, data.start_col,
                             data.end_col, data.is_rectangular);
}

//
// Paste content from named buffer.
//
bool Editor::paste_macro_buffer(char name)
{
    auto it = macros_.find(name);
    if (it == macros_.end() || !it->second.is_buffer())
        return false;

    // Restore clipboard from macro buffer
    auto data = it->second.get_all_buffer_data();
    clipboard_.set_data(data.is_rectangular, data.start_line, data.end_line, data.start_col,
                        data.end_col, data.lines);

    // Paste the buffer
    if (!clipboard_.is_empty()) {
        // TODO: Implement clipboard paste with segments
        // Use the clipboard's paste method
        /*
        if (!clipboard_.is_rectangular()) {
            clipboard_.paste_into_lines(lines, curLine);
        } else {
            clipboard_.paste_into_rectangular(lines, curLine, curCol);
        }
        */

        ensure_cursor_visible();
    }
    return true;
}

//
// Define text area using stored tag position.
//
bool Editor::mdeftag(char tag_name)
{
    auto it = macros_.find(tag_name);
    if (it == macros_.end() || !it->second.is_position()) {
        status_ = "Tag not found";
        return false;
    }

    // Get current cursor position
    int cur_line = wksp_->view.topline + cursor_line_;
    int cur_col  = wksp_->view.basecol + cursor_col_;

    // Get tag position
    auto pos     = it->second.get_position();
    int tag_line = pos.first;
    int tag_col  = pos.second;

    // Set up area between current cursor and tag
    params_.type = Parameters::PARAM_TAG_AREA;
    params_.c0   = cur_col;
    params_.r0   = cur_line;
    params_.c1   = tag_col;
    params_.r1   = tag_line;

    // Normalize bounds
    bool needs_reposition = false;
    bool was_swapped      = false;

    if (params_.type == Parameters::PARAM_TAG_AREA) {
        // Get original coordinates
        int start_col            = params_.c0;
        int start_row            = params_.r0;
        bool was_start_at_cursor = (cur_line == start_row && cur_col == start_col);

        params_.normalize_area();

        // Check if coordinates were swapped
        int new_start_col = params_.c0;
        int new_start_row = params_.r0;
        was_swapped       = (new_start_row != cur_line || new_start_col != cur_col);

        if (was_swapped) {
            needs_reposition = was_start_at_cursor;
        }
    }

    // Determine message based on selection type
    if (params_.is_horizontal_area()) {
        status_ = "*** Columns defined by tag ***";
    } else if (params_.is_vertical_area()) {
        status_ = "*** Lines defined by tag ***";
    } else {
        status_ = "*** Area defined by tag ***";
    }

    // Move cursor to start if needed
    if (needs_reposition) {
        goto_line(params_.r0);
        wksp_->view.basecol = params_.c0;
        cursor_col_         = 0;
        ensure_cursor_visible();
    }

    return true;
}
