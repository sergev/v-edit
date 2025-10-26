#include "editor.h"

//
// Copy specified lines to clipboard.
//
void Editor::picklines(int startLine, int count)
{
    if (count <= 0 || startLine < 0) {
        return;
    }

    ensure_line_saved(); // Save any unsaved modifications

    // Read lines from workspace segments
    std::vector<std::string> lines;
    for (int i = 0; i < count && (startLine + i) < wksp->nlines(); ++i) {
        std::string line = read_line_from_wksp(startLine + i);
        lines.push_back(line);
    }

    // Store in clipboard
    clipboard.copy_lines(lines, 0, lines.size());
}

//
// Insert clipboard content at specified position.
//
void Editor::paste(int afterLine, int atCol)
{
    if (clipboard.is_empty()) {
        return;
    }

    ensure_line_saved(); // Save any unsaved modifications

    const std::vector<std::string> &clip_lines = clipboard.get_lines();

    if (clipboard.is_rectangular()) {
        // Paste as rectangular block - insert at column position
        for (size_t i = 0; i < clip_lines.size() && (afterLine + (int)i) < wksp->nlines(); ++i) {
            get_line(afterLine + i);
            if (atCol < (int)current_line.size()) {
                current_line.insert(atCol, clip_lines[i]);
            } else {
                // Extend line with spaces if needed
                current_line.resize(atCol, ' ');
                current_line += clip_lines[i];
            }
            current_line_modified = true;
            put_line();
        }
    } else {
        // Paste as lines
        for (const auto &line : clip_lines) {
            // Create a segment for this line
            Segment *new_seg = write_line_to_temp(line);
            if (new_seg) {
                wksp->insert_segments(new_seg, afterLine + 1);
                afterLine++;
                wksp->set_nlines(wksp->nlines() + 1);
            }
        }
    }

    ensure_cursor_visible();
}

//
// Copy rectangular block to clipboard.
//
void Editor::pickspaces(int line, int col, int number, int nl)
{
    if (number <= 0 || nl <= 0 || line < 0 || col < 0) {
        return;
    }

    ensure_line_saved(); // Save any unsaved modifications

    // Read lines from workspace and extract rectangular block
    std::vector<std::string> lines;
    for (int i = 0; i < nl && (line + i) < wksp->nlines(); ++i) {
        std::string full_line = read_line_from_wksp(line + i);
        std::string block;

        if (col < (int)full_line.size()) {
            int end_col = std::min(col + number, (int)full_line.size());
            block       = full_line.substr(col, end_col - col);
        }

        lines.push_back(block);
    }

    // Store rectangular block in clipboard
    clipboard.copy_rectangular_block(lines, 0, col, number, lines.size());
}

//
// Delete rectangular block and save to clipboard.
//
void Editor::closespaces(int line, int col, int number, int nl)
{
    // Delete rectangular area and save to clipboard
    pickspaces(line, col, number, nl); // copy first

    // Now delete the rectangular area using get_line/put_line pattern
    ensure_line_saved();
    for (int i = 0; i < nl; ++i) {
        if (line + i < wksp->nlines()) {
            get_line(line + i);
            if (col < (int)current_line.size()) {
                int end_pos = std::min(col + number, (int)current_line.size());
                current_line.erase(col, end_pos - col);
                current_line_modified = true;
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
        if (line + i < wksp->nlines()) {
            get_line(line + i);
            if (col <= (int)current_line.size()) {
                current_line.insert(col, number, ' ');
            } else {
                // Extend line with spaces if needed
                current_line.resize(col, ' ');
                current_line.insert(col, number, ' ');
            }
            current_line_modified = true;
            put_line();
        } else {
            // Create new line if needed - TODO: need proper implementation
            Segment *blank = wksp->create_blank_lines(1);
            wksp->insert_segments(blank, line + i);
        }
    }
    ensure_cursor_visible();
}

//
// Save current cursor position to named macro.
//
void Editor::save_macro_position(char name)
{
    int absLine = wksp->topline() + cursor_line;
    int absCol  = wksp->basecol() + cursor_col;
    macros[name].setPosition(absLine, absCol);
}

//
// Navigate to position stored in named macro.
//
bool Editor::goto_macro_position(char name)
{
    auto it = macros.find(name);
    if (it == macros.end() || !it->second.isPosition())
        return false;
    auto pos = it->second.getPosition();
    goto_line(pos.first);
    wksp->set_basecol(pos.second);
    cursor_col = 0;
    ensure_cursor_visible();
    return true;
}

//
// Save current clipboard to named buffer.
//
void Editor::save_macro_buffer(char name)
{
    // Save current clipboard content to named macro buffer
    auto data = clipboard.get_data();
    macros[name].setBuffer(data.lines, data.start_line, data.end_line, data.start_col, data.end_col,
                           data.is_rectangular);
}

//
// Paste content from named buffer.
//
bool Editor::paste_macro_buffer(char name)
{
    auto it = macros.find(name);
    if (it == macros.end() || !it->second.isBuffer())
        return false;

    // Restore clipboard from macro buffer
    auto data = it->second.getAllBufferData();
    clipboard.set_data(data.is_rectangular, data.start_line, data.end_line, data.start_col,
                       data.end_col, data.lines);

    // Paste the buffer
    if (!clipboard.is_empty()) {
        // TODO: Implement clipboard paste with segments
        // Use the clipboard's paste method
        /*
        if (!clipboard.is_rectangular()) {
            clipboard.paste_into_lines(lines, curLine);
        } else {
            clipboard.paste_into_rectangular(lines, curLine, curCol);
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
    auto it = macros.find(tag_name);
    if (it == macros.end() || !it->second.isPosition()) {
        status = "Tag not found";
        return false;
    }

    // Get current cursor position
    int curLine = wksp->topline() + cursor_line;
    int curCol  = wksp->basecol() + cursor_col;

    // Get tag position
    auto pos    = it->second.getPosition();
    int tagLine = pos.first;
    int tagCol  = pos.second;

    // Set up area between current cursor and tag
    param_type = -2;
    param_r0   = curLine;
    param_c0   = curCol;
    param_r1   = tagLine;
    param_c1   = tagCol;

    // Normalize bounds (swap if needed)
    int f = 0;
    if (param_r0 > param_r1) {
        std::swap(param_r0, param_r1);
        f++;
    }
    if (param_c0 > param_c1) {
        std::swap(param_c0, param_c1);
        f++;
    }

    // Determine message based on selection type
    if (param_r1 == param_r0) {
        status = "*** Columns defined by tag ***";
    } else if (param_c1 == param_c0) {
        status = "*** Lines defined by tag ***";
    } else {
        status = "*** Area defined by tag ***";
    }

    // Move cursor to start if swapped
    if (f) {
        goto_line(param_r0);
        wksp->set_basecol(param_c0);
        cursor_col = 0;
        ensure_cursor_visible();
    }

    return true;
}
