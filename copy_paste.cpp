#include "editor.h"

//
// Copy specified lines to clipboard_.
//
void Editor::picklines(int startLine, int count)
{
    if (count <= 0 || startLine < 0) {
        return;
    }

    ensure_line_saved(); // Save any unsaved modifications

    // Read lines from workspace segments
    std::vector<std::string> lines;
    for (int i = 0; i < count && (startLine + i) < wksp_->nlines(); ++i) {
        std::string line = read_line_from_wksp(startLine + i);
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

    ensure_line_saved(); // Save any unsaved modifications

    const std::vector<std::string> &clip_lines = clipboard_.get_lines();

    if (clipboard_.is_rectangular()) {
        // Paste as rectangular block - insert at column position
        for (size_t i = 0; i < clip_lines.size() && (afterLine + (int)i) < wksp_->nlines(); ++i) {
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
        for (const auto &line : clip_lines) {
            // Create a segment for this line
            auto temp_segments = tempfile_.write_line_to_temp(line);
            if (!temp_segments.empty()) {
                // Splice the segments into workspace at the correct position
                auto insert_pos = wksp_->get_segments().begin();
                std::advance(insert_pos, afterLine + 1); // Position after the target line
                wksp_->get_segments().splice(insert_pos, temp_segments);
                afterLine++;
                wksp_->set_nlines(wksp_->nlines() + 1);
            }
        }
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

    ensure_line_saved(); // Save any unsaved modifications

    // Read lines from workspace and extract rectangular block
    std::vector<std::string> lines;
    for (int i = 0; i < nl && (line + i) < wksp_->nlines(); ++i) {
        std::string full_line = read_line_from_wksp(line + i);
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
    ensure_line_saved();
    for (int i = 0; i < nl; ++i) {
        if (line + i < wksp_->nlines()) {
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
    ensure_line_saved();
    for (int i = 0; i < nl; ++i) {
        if (line + i < wksp_->nlines()) {
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
            wksp_->insert_segments(blank, line + i);
        }
    }
    ensure_cursor_visible();
}

//
// Save current cursor position to named macro.
//
void Editor::save_macro_position(char name)
{
    int absLine = wksp_->topline() + cursor_line_;
    int absCol  = wksp_->basecol() + cursor_col_;
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
    wksp_->set_basecol(pos.second);
    cursor_col_ = 0;
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
    int curLine = wksp_->topline() + cursor_line_;
    int curCol  = wksp_->basecol() + cursor_col_;

    // Get tag position
    auto pos    = it->second.getPosition();
    int tagLine = pos.first;
    int tagCol  = pos.second;

    // Set up area between current cursor and tag
    params_.set_type(Parameters::PARAM_TAG_AREA);
    params_.set_area_start(curCol, curLine);
    params_.set_area_end(tagCol, tagLine);

    // Normalize bounds
    bool needs_reposition = false;
    bool was_swaped       = false;

    if (params_.get_type() == Parameters::PARAM_TAG_AREA) {
        // Get original coordinates
        int start_col, start_row;
        params_.get_area_start(start_col, start_row);
        bool was_start_at_cursor = (curLine == start_row && curCol == start_col);

        params_.normalize_area();

        // Check if coordinates were swapped
        int new_start_col, new_start_row;
        params_.get_area_start(new_start_col, new_start_row);
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
        int col, row;
        params_.get_area_start(col, row);
        goto_line(row);
        wksp_->set_basecol(col);
        cursor_col_ = 0;
        ensure_cursor_visible();
    }

    return true;
}
