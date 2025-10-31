#include "editor.h"

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
