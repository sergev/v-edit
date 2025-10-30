#include "editor.h"

//
// Copy specified lines to clipboard_.
//
void Editor::picklines(int startLine, int count)
{
    if (count <= 0 || startLine < 0) {
        return;
    }

    put_line(); // Save any unsaved modifications

    // Read lines from workspace segments
    std::vector<std::string> lines;
    auto total = wksp_->total_line_count();
    for (int i = 0; i < count && (startLine + i) < total; ++i) {
        std::string line = wksp_->read_line(startLine + i);
        lines.push_back(line);
    }

    // Store in clipboard
    clipboard_.copy_lines(lines, 0, lines.size());
}

//
// Insert clipboard content at specified position.
//
void Editor::paste(int afterLine, int atCol)
{
    if (clipboard_.is_empty()) {
        return;
    }

    put_line(); // Save any unsaved modifications

    const std::vector<std::string> &clip_lines = clipboard_.get_lines();

    if (clipboard_.is_rectangular()) {
        // Paste as rectangular block - insert at column position
        auto total = wksp_->total_line_count();
        for (size_t i = 0; i < clip_lines.size() && (afterLine + (int)i) < total; ++i) {
            get_line(afterLine + i);
            if (atCol < (int)current_line_.size()) {
                current_line_.insert(atCol, clip_lines[i]);
            } else {
                // Extend line with spaces if needed
                current_line_.resize(atCol, ' ');
                current_line_ += clip_lines[i];
            }
            current_line_modified_ = true;
            put_line();
        }
    } else {
        // Paste as lines
        auto clip_segments = tempfile_.write_lines_to_temp(clip_lines);

        // Insert the segments into workspace at the correct position
        wksp_->insert_contents(clip_segments, afterLine);
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

//
// Save current cursor position to named macro.
//
void Editor::save_macro_position(char name)
{
    int absLine = wksp_->view.topline + cursor_line_;
    int absCol  = wksp_->view.basecol + cursor_col_;
    macros_[name].setPosition(absLine, absCol);
}

//
// Navigate to position stored in named macro.
//
bool Editor::goto_macro_position(char name)
{
    auto it = macros_.find(name);
    if (it == macros_.end() || !it->second.isPosition())
        return false;
    auto pos = it->second.getPosition();
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
    macros_[name].setBuffer(data.lines, data.start_line, data.end_line, data.start_col,
                            data.end_col, data.is_rectangular);
}

//
// Paste content from named buffer.
//
bool Editor::paste_macro_buffer(char name)
{
    auto it = macros_.find(name);
    if (it == macros_.end() || !it->second.isBuffer())
        return false;

    // Restore clipboard from macro buffer
    auto data = it->second.getAllBufferData();
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
    if (it == macros_.end() || !it->second.isPosition()) {
        status_ = "Tag not found";
        return false;
    }

    // Get current cursor position
    int curLine = wksp_->view.topline + cursor_line_;
    int curCol  = wksp_->view.basecol + cursor_col_;

    // Get tag position
    auto pos    = it->second.getPosition();
    int tagLine = pos.first;
    int tagCol  = pos.second;

    // Set up area between current cursor and tag
    params_.type = Parameters::PARAM_TAG_AREA;
    params_.c0 = curCol;
    params_.r0 = curLine;
    params_.c1 = tagCol;
    params_.r1 = tagLine;

    // Normalize bounds
    bool needs_reposition = false;
    bool was_swaped       = false;

    if (params_.type == Parameters::PARAM_TAG_AREA) {
        // Get original coordinates
        int start_col = params_.c0;
        int start_row = params_.r0;
        bool was_start_at_cursor = (curLine == start_row && curCol == start_col);

        params_.normalize_area();

        // Check if coordinates were swapped
        int new_start_col = params_.c0;
        int new_start_row = params_.r0;
        was_swaped = (new_start_row != curLine || new_start_col != curCol);

        if (was_swaped) {
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
