#include "editor.h"

//
// Copy specified lines to clipboard.
//
void Editor::picklines(int startLine, int count)
{
    clipboard.copy_lines(lines, startLine, count);
}

//
// Insert clipboard content at specified position.
//
void Editor::paste(int afterLine, int atCol)
{
    if (clipboard.is_empty()) {
        return;
    }

    if (!clipboard.is_rectangular()) {
        // Paste as lines
        clipboard.paste_into_lines(lines, afterLine);
    } else {
        // Paste as rectangular block
        clipboard.paste_into_rectangular(lines, afterLine, atCol);
    }

    build_segment_chain_from_lines();
    ensure_cursor_visible();
}

//
// Copy rectangular block to clipboard.
//
void Editor::pickspaces(int line, int col, int number, int nl)
{
    clipboard.copy_rectangular_block(lines, line, col, number, nl);
}

//
// Delete rectangular block and save to clipboard.
//
void Editor::closespaces(int line, int col, int number, int nl)
{
    // Delete rectangular area and save to clipboard
    pickspaces(line, col, number, nl); // copy first

    // Now delete the rectangular area
    for (int i = 0; i < nl; ++i) {
        if (line + i < (int)lines.size()) {
            std::string &ln = lines[line + i];
            if (col < (int)ln.size()) {
                int end_pos = std::min(col + number, (int)ln.size());
                ln.erase(col, end_pos - col);
            }
        }
    }
    build_segment_chain_from_lines();
    ensure_cursor_visible();
}

//
// Insert spaces into rectangular area.
//
void Editor::openspaces(int line, int col, int number, int nl)
{
    // Insert spaces in rectangular area
    for (int i = 0; i < nl; ++i) {
        if (line + i < (int)lines.size()) {
            std::string &ln = lines[line + i];
            if (col <= (int)ln.size()) {
                ln.insert(col, number, ' ');
            } else {
                // Extend line with spaces if needed
                ln.resize(col, ' ');
                ln.insert(col, number, ' ');
            }
        } else {
            // Create new line if needed
            while ((int)lines.size() <= line + i) {
                lines.push_back("");
            }
            lines[line + i].resize(col, ' ');
            lines[line + i].insert(col, number, ' ');
        }
    }
    build_segment_chain_from_lines();
    ensure_cursor_visible();
}

//
// Save current cursor position to named macro.
//
void Editor::save_macro_position(char name)
{
    int absLine = wksp.topline() + cursor_line;
    int absCol  = wksp.basecol() + cursor_col;
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
    wksp.set_basecol(pos.second);
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
        int curLine = wksp.topline() + cursor_line;
        int curCol  = wksp.basecol() + cursor_col;

        // Use the clipboard's paste method
        if (!clipboard.is_rectangular()) {
            clipboard.paste_into_lines(lines, curLine);
        } else {
            clipboard.paste_into_rectangular(lines, curLine, curCol);
        }

        build_segment_chain_from_lines();
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
    int curLine = wksp.topline() + cursor_line;
    int curCol  = wksp.basecol() + cursor_col;

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
        wksp.set_basecol(param_c0);
        cursor_col = 0;
        ensure_cursor_visible();
    }

    return true;
}
