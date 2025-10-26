#include "editor.h"

//
// Save current cursor position to named macro.
//
void Editor::save_macro_position(char name)
{
    int absLine           = wksp.topline() + cursor_line;
    int absCol            = wksp.basecol() + cursor_col;
    macros[name].type     = Macro::POSITION;
    macros[name].position = std::make_pair(absLine, absCol);
}

//
// Navigate to position stored in named macro.
//
bool Editor::goto_macro_position(char name)
{
    auto it = macros.find(name);
    if (it == macros.end() || it->second.type != Macro::POSITION)
        return false;
    goto_line(it->second.position.first);
    wksp.set_basecol(it->second.position.second);
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
    macros[name].type           = Macro::BUFFER;
    macros[name].buffer_lines   = clipboard.lines;
    macros[name].start_line     = clipboard.start_line;
    macros[name].end_line       = clipboard.end_line;
    macros[name].start_col      = clipboard.start_col;
    macros[name].end_col        = clipboard.end_col;
    macros[name].is_rectangular = clipboard.is_rectangular;
}

//
// Paste content from named buffer.
//
bool Editor::paste_macro_buffer(char name)
{
    auto it = macros.find(name);
    if (it == macros.end() || it->second.type != Macro::BUFFER)
        return false;

    // Restore clipboard from macro buffer
    clipboard.lines          = it->second.buffer_lines;
    clipboard.start_line     = it->second.start_line;
    clipboard.end_line       = it->second.end_line;
    clipboard.start_col      = it->second.start_col;
    clipboard.end_col        = it->second.end_col;
    clipboard.is_rectangular = it->second.is_rectangular;

    // Paste the buffer
    if (!clipboard.is_empty()) {
        int curLine = wksp.topline() + cursor_line;
        lines.insert(lines.begin() + std::min((int)lines.size(), curLine + 1),
                     clipboard.lines.begin(), clipboard.lines.end());
        build_segment_chain_from_lines();
        ensure_cursor_visible();
    }
    return true;
}

//
// Copy specified lines to clipboard.
//
void Editor::picklines(int startLine, int count)
{
    clipboard.clear();
    clipboard.is_rectangular = false;
    clipboard.start_line     = startLine;
    clipboard.end_line       = startLine + count - 1;

    for (int i = 0; i < count; ++i) {
        if (startLine + i < (int)lines.size()) {
            clipboard.lines.push_back(lines[startLine + i]);
        }
    }
}

//
// Insert clipboard content at specified position.
//
void Editor::paste(int afterLine, int atCol)
{
    if (clipboard.is_empty()) {
        return;
    }

    if (!clipboard.is_rectangular) {
        // Paste as lines
        lines.insert(lines.begin() + std::min((int)lines.size(), afterLine + 1),
                     clipboard.lines.begin(), clipboard.lines.end());
    } else {
        // Paste as rectangular block
        int startLine = afterLine + 1;
        int numLines  = clipboard.lines.size();
        int numCols   = clipboard.end_col - clipboard.start_col + 1;

        // Ensure we have enough lines
        while ((int)lines.size() <= startLine + numLines - 1) {
            lines.push_back("");
        }

        // Insert the rectangular block
        for (int i = 0; i < numLines; ++i) {
            std::string &targetLine = lines[startLine + i];

            // Expand line if necessary
            if ((int)targetLine.size() < atCol + numCols) {
                targetLine.resize(atCol + numCols, ' ');
            }

            // Copy the block content
            if (i < (int)clipboard.lines.size()) {
                const std::string &src = clipboard.lines[i];
                for (int j = 0; j < numCols && j < (int)src.size(); ++j) {
                    targetLine[atCol + j] = src[j];
                }
            }
        }
    }

    build_segment_chain_from_lines();
    ensure_cursor_visible();
}

//
// Copy rectangular block to clipboard.
//
void Editor::pickspaces(int line, int col, int number, int nl)
{
    // Copy rectangular area to clipboard
    clipboard.clear();
    clipboard.is_rectangular = true;
    clipboard.start_line     = line;
    clipboard.end_line       = line + nl - 1;
    clipboard.start_col      = col;
    clipboard.end_col        = col + number - 1;

    for (int i = 0; i < nl; ++i) {
        if (line + i < (int)lines.size()) {
            std::string &ln = lines[line + i];
            std::string block_line;
            for (int j = 0; j < number; ++j) {
                if (col + j < (int)ln.size()) {
                    block_line += ln[col + j];
                } else {
                    block_line += ' '; // pad with spaces
                }
            }
            clipboard.lines.push_back(block_line);
        } else {
            clipboard.lines.push_back(std::string(number, ' '));
        }
    }
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
// Define text area using stored tag position.
//
bool Editor::mdeftag(char tag_name)
{
    auto it = macros.find(tag_name);
    if (it == macros.end() || it->second.type != Macro::POSITION) {
        status = "Tag not found";
        return false;
    }

    // Get current cursor position
    int curLine = wksp.topline() + cursor_line;
    int curCol  = wksp.basecol() + cursor_col;

    // Get tag position
    int tagLine = it->second.position.first;
    int tagCol  = it->second.position.second;

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
