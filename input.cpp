#include <ncurses.h>

#include <iostream>

#include "editor.h"

void Editor::handle_key(int ch)
{
    if (cmd_mode) {
        handle_key_cmd(ch);
        return;
    }
    handle_key_edit(ch);
}

void Editor::handle_key_cmd(int ch)
{
    // Handle rectangular block operations if area is being selected
    if (area_selection_mode) {
        if (ch == 3) { // ^C - Copy rectangular block
            // Finalize area bounds
            if (param_c0 > param_c1) {
                std::swap(param_c0, param_c1);
            }
            if (param_r0 > param_r1) {
                std::swap(param_r0, param_r1);
            }
            int numCols  = param_c1 - param_c0 + 1;
            int numLines = param_r1 - param_r0 + 1;
            pickspaces(param_r0, param_c0, numCols, numLines);
            
            // Check if we should store to a named buffer (>name)
            if (!cmd.empty() && cmd[0] == '>' && cmd.size() == 2 && cmd[1] >= 'a' && cmd[1] <= 'z') {
                save_macro_buffer(cmd[1]);
                status = std::string("Copied and saved to buffer '") + cmd[1] + "'";
            } else {
                status = "Copied rectangular block";
            }
            
            cmd_mode            = false;
            area_selection_mode = false;
            cmd.clear();
            param_type          = 0;
            return;
        }
        if (ch == 25) { // ^Y - Delete rectangular block
            // Finalize area bounds
            if (param_c0 > param_c1) {
                std::swap(param_c0, param_c1);
            }
            if (param_r0 > param_r1) {
                std::swap(param_r0, param_r1);
            }
            int numCols  = param_c1 - param_c0 + 1;
            int numLines = param_r1 - param_r0 + 1;
            closespaces(param_r0, param_c0, numCols, numLines);
            
            // Check if we should store to a named buffer (>name)
            if (!cmd.empty() && cmd[0] == '>' && cmd.size() == 2 && cmd[1] >= 'a' && cmd[1] <= 'z') {
                save_macro_buffer(cmd[1]);
                status = std::string("Deleted and saved to buffer '") + cmd[1] + "'";
            } else {
                status = "Deleted rectangular block";
            }
            
            cmd_mode            = false;
            area_selection_mode = false;
            cmd.clear();
            param_type          = 0;
            return;
        }
        if (ch == 15) { // ^O - Insert rectangular block of spaces
            // Finalize area bounds
            if (param_c0 > param_c1) {
                std::swap(param_c0, param_c1);
            }
            if (param_r0 > param_r1) {
                std::swap(param_r0, param_r1);
            }
            int numCols  = param_c1 - param_c0 + 1;
            int numLines = param_r1 - param_r0 + 1;
            openspaces(param_r0, param_c0, numCols, numLines);
            status              = "Inserted rectangular spaces";
            cmd_mode            = false;
            area_selection_mode = false;
            cmd.clear();
            param_type          = 0;
            return;
        }
    }

    // Check if this is a movement key (area selection)
    if (is_movement_key(ch)) {
        if (!area_selection_mode) {
            // Start area selection
            area_selection_mode = true;
            param_c0 = param_c1 = wksp.offset + cursor_col;
            param_r0 = param_r1 = wksp.topline + cursor_line;
            status              = "*** Area defined by cursor ***";
        }
        handle_area_selection(ch);
        return;
    }

    // Handle Enter key in area selection - it just moves cursor (start a new line)
    if ((ch == '\n' || ch == KEY_ENTER) && area_selection_mode) {
        // Enter just moves the cursor, doesn't finalize
        move_down();
        handle_area_selection(ch);
        return;
    }

    if (ch == 27 || ch == KEY_F(1) || ch == 1) {
        // ESC or F1 or ^A cancels area selection
        if (area_selection_mode) {
            area_selection_mode = false;
            param_type          = 0;
            cmd_mode            = false;
            status              = "Cancelled";
            return;
        }
    }

    if (ch == 27) {
        cmd.clear();
        filter_mode         = false;
        area_selection_mode = false;
        param_type          = 0;
        return;
    } // ESC
    if (ch == '\n' || ch == KEY_ENTER) {
        if (area_selection_mode) {
            // This shouldn't happen anymore as we handle Enter above
            return;
        }
        // Parse numeric count if command starts with a number
        if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
            size_t i = 0;
            while (i < cmd.size() && cmd[i] >= '0' && cmd[i] <= '9') {
                i++;
            }
            if (i > 0) {
                param_count = std::atoi(cmd.substr(0, i).c_str());
                cmd = cmd.substr(i);
            }
        }
        if (!cmd.empty()) {
            if (cmd == "qa") {
                quit_flag = true;
                status    = "Exiting";
            } else if (cmd == "ad") {
                quit_flag = true;
                status    = "ABORTED";
            } else if (cmd == "q") {
                quit_flag = true;
                status    = "Exiting";
            } else if (cmd == "r") {
                // Redraw screen
                wksp_redraw();
                status = "Redrawn";
            } else if (cmd.size() >= 2 && cmd.substr(0, 2) == "w ") {
                // w + to make writable (or other w commands)
                if (cmd.size() >= 3 && cmd[2] == '+') {
                    if (!files.empty() && wksp.wfile < (int)files.size()) {
                        files[wksp.wfile].writable = 1;
                        status = "File marked writable";
                    }
                } else {
                    if (!files.empty() && wksp.wfile < (int)files.size()) {
                        files[wksp.wfile].writable = 0;
                        status = "File marked read-only";
                    }
                }
            } else if (cmd == "s") {
                save_file();
            } else if (cmd.size() > 1 && cmd[0] == 's' && cmd[1] != ' ') {
                // s<filename> - save as
                std::string new_filename = cmd.substr(1);
                save_as(new_filename);
            } else if (cmd.size() > 1 && cmd[0] == 'o') {
                // Open file: o<filename>
                std::string file_to_open = cmd.substr(1);
                if (open_file(file_to_open)) {
                    status = std::string("Opened: ") + file_to_open;
                } else {
                    status = std::string("Failed to open: ") + file_to_open;
                }
            } else if (filter_mode && !cmd.empty()) {
                // External filter command
                int curLine  = wksp.topline + cursor_line;
                int numLines = 1; // default to current line

                // Parse command for line count (e.g., "3 sort" means sort 3 lines)
                std::string command = cmd;
                size_t spacePos     = cmd.find(' ');
                if (spacePos != std::string::npos) {
                    std::string countStr = cmd.substr(0, spacePos);
                    if (countStr.find_first_not_of("0123456789") == std::string::npos) {
                        numLines = std::atoi(countStr.c_str());
                        if (numLines < 1)
                            numLines = 1;
                        command = cmd.substr(spacePos + 1);
                        // Trim leading spaces from command
                        while (!command.empty() && command[0] == ' ') {
                            command = command.substr(1);
                        }
                    }
                } else {
                    // No space found, check if command starts with a number
                    if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
                        // Extract number from beginning
                        size_t i = 0;
                        while (i < cmd.size() && cmd[i] >= '0' && cmd[i] <= '9') {
                            i++;
                        }
                        if (i > 0) {
                            std::string countStr = cmd.substr(0, i);
                            numLines             = std::atoi(countStr.c_str());
                            if (numLines < 1)
                                numLines = 1;
                            command = cmd.substr(i);
                        }
                    }
                }

                // Debug: show what we're executing
                status = std::string("Executing: ") + command + " on " + std::to_string(numLines) +
                         " lines";

                if (execute_external_filter(command, curLine, numLines)) {
                    status = std::string("Filtered ") + std::to_string(numLines) + " line(s)";
                } else {
                    status = "Filter execution failed";
                }
                filter_mode = false;
            } else if (cmd.size() > 1 && cmd[0] == 'g') {
                // goto line: g<number>
                int ln = std::atoi(cmd.c_str() + 1);
                if (ln < 1)
                    ln = 1;
                goto_line(ln - 1);
            } else if (cmd.size() > 1 && cmd[0] == '/') {
                // search: /text
                std::string needle = cmd.substr(1);
                if (!needle.empty()) {
                    last_search_forward = true;
                    last_search         = needle;
                    search_forward(needle);
                }
            } else if (cmd.size() > 1 && cmd[0] == '?') {
                // backward search: ?text
                std::string needle = cmd.substr(1);
                if (!needle.empty()) {
                    last_search_forward = false;
                    last_search         = needle;
                    search_backward(needle);
                }
            } else if (cmd == "n") {
                if (last_search_forward)
                    search_next();
                else
                    search_prev();
            } else if (cmd.size() == 2 && cmd[0] == '>' && cmd[1] >= 'a' && cmd[1] <= 'z') {
                // Save macro buffer: >x (saves current clipboard to named buffer)
                save_macro_buffer(cmd[1]);
                status = std::string("Buffer '") + cmd[1] + "' saved";
            } else if (cmd.size() == 3 && cmd[0] == '>' && cmd[1] == '>' && cmd[2] >= 'a' &&
                       cmd[2] <= 'z') {
                // Save macro position: >>x (saves current position)
                save_macro_position(cmd[2]);
                status = std::string("Position '") + cmd[2] + "' saved";
            } else if (cmd.size() == 2 && cmd[0] == '$' && cmd[1] >= 'a' && cmd[1] <= 'z') {
                // Use macro: $x (tries buffer first, then position)
                // Check if in area selection mode for mdeftag
                if (area_selection_mode) {
                    // Define area using tag
                    mdeftag(cmd[1]);
                    // Stay in area selection mode for further commands
                    return;
                }
                auto it = macros.find(cmd[1]);
                if (it != macros.end()) {
                    if (it->second.type == Macro::BUFFER) {
                        if (paste_macro_buffer(cmd[1])) {
                            status = std::string("Pasted buffer '") + cmd[1] + "'";
                        } else {
                            status = std::string("Buffer '") + cmd[1] + "' empty";
                        }
                    } else if (it->second.type == Macro::POSITION) {
                        if (goto_macro_position(cmd[1])) {
                            status = std::string("Goto position '") + cmd[1] + "'";
                        } else {
                            status = std::string("Position '") + cmd[1] + "' not found";
                        }
                    }
                } else {
                    status = std::string("Macro '") + cmd[1] + "' not found";
                }
            } else if (cmd.size() >= 1 && cmd[0] == ':') {
                // Colon commands: :w, :q, :wq, :<number>
                if (cmd == ":w") {
                    save_file();
                } else if (cmd == ":q") {
                    quit_flag = true;
                    status    = "Exiting";
                } else if (cmd == ":wq") {
                    save_file();
                    quit_flag = true;
                    status    = "Saved and exiting";
                } else if (cmd.size() > 1 && cmd[0] == ':') {
                    // :<number> - goto line
                    int ln = std::atoi(cmd.c_str() + 1);
                    if (ln >= 1) {
                        goto_line(ln - 1);
                        status = std::string("Goto line ") + std::to_string(ln);
                    }
                }
            } else if (cmd.size() >= 1 && cmd[0] >= '0' && cmd[0] <= '9') {
                // Direct line number - goto line
                int ln = std::atoi(cmd.c_str());
                if (ln >= 1) {
                    goto_line(ln - 1);
                    status = std::string("Goto line ") + cmd;
                }
            }
        }
        // Clear param_count at end of command
        param_count = 0;
        cmd_mode            = false;
        filter_mode         = false;
        area_selection_mode = false;
        cmd.clear();
        param_type = 0;
        return;
    }
    
    // Handle control characters with potential count in command mode
    if (cmd_mode && !area_selection_mode && !filter_mode) {
        if (ch == 3) { // ^C - Copy
            int curLine = wksp.topline + cursor_line;
            int count = param_count > 0 ? param_count : 1;
            picklines(curLine, count);
            status = std::string("Copied ") + std::to_string(count) + " line(s)";
            cmd_mode = false;
            param_count = 0;
            return;
        }
        if (ch == 25) { // ^Y - Delete
            int curLine = wksp.topline + cursor_line;
            int count = param_count > 0 ? param_count : 1;
            deletelines(curLine, count);
            status = std::string("Deleted ") + std::to_string(count) + " line(s)";
            cmd_mode = false;
            param_count = 0;
            return;
        }
        if (ch == 15) { // ^O - Insert blank lines
            int curLine = wksp.topline + cursor_line;
            int count = param_count > 0 ? param_count : 1;
            insertlines(curLine, count);
            status = std::string("Inserted ") + std::to_string(count) + " line(s)";
            cmd_mode = false;
            param_count = 0;
            return;
        }
    }
    if (ch == KEY_BACKSPACE || ch == 127) {
        if (!cmd.empty())
            cmd.pop_back();
        return;
    }
    if (ch >= 32 && ch < 127)
        cmd.push_back((char)ch);
}

// Enhanced parameter system functions
bool Editor::is_movement_key(int ch) const
{
    return (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN || ch == KEY_HOME ||
            ch == KEY_END || ch == KEY_PPAGE || ch == KEY_NPAGE) &&
           ch != '\n' && ch != '\r' && ch != KEY_ENTER;
}

void Editor::handle_area_selection(int ch)
{
    // Move cursor based on key
    switch (ch) {
    case KEY_LEFT:
        move_left();
        break;
    case KEY_RIGHT:
        move_right();
        break;
    case KEY_UP:
        move_up();
        break;
    case KEY_DOWN:
        move_down();
        break;
    case KEY_HOME:
        cursor_col = 0;
        break;
    case KEY_END:
        if (cursor_line < (int)lines.size()) {
            cursor_col = lines[cursor_line].length();
        }
        break;
    case KEY_PPAGE:
        for (int i = 0; i < 10; i++)
            move_up();
        break;
    case KEY_NPAGE:
        for (int i = 0; i < 10; i++)
            move_down();
        break;
    }

    // Update area bounds
    int curCol  = wksp.offset + cursor_col;
    int curLine = wksp.topline + cursor_line;

    if (curCol > param_c0) {
        param_c1 = curCol;
    } else {
        param_c0 = curCol;
    }

    if (curLine > param_r0) {
        param_r1 = curLine;
    } else {
        param_r0 = curLine;
    }
}

void Editor::enter_command_mode()
{
    cmd_mode            = true;
    area_selection_mode = false;
    param_type          = 0;
    param_count         = 0;
    param_str.clear();
    cmd.clear();
    status = "Cmd: ";
}

void Editor::handle_key_edit(int ch)
{
    if (ch == KEY_F(1) || ch == 1) { // F1 or ^A
        enter_command_mode();
        return;
    }
    if (ch == KEY_F(2)) {
        save_file();
        return;
    }
    if (ch == KEY_F(3)) {
        // Next file / Switch file
        next_file();
        return;
    }
    if (ch == KEY_F(4)) {
        // External filter execution - enter command mode to get command
        cmd_mode    = true;
        filter_mode = true;
        cmd         = "";
        status      = "Filter command: ";
        return;
    }
    if (ch == KEY_F(5)) {
        // Copy current line to clipboard
        int curLine = wksp.topline + cursor_line;
        clipboard.clear();
        if (curLine >= 0 && curLine < (int)lines.size()) {
            clipboard.lines.push_back(lines[curLine]);
            clipboard.start_line = curLine;
            clipboard.end_line   = curLine;
        }
        status = "Copied";
        return;
    }
    if (ch == KEY_F(6)) {
        // Paste clipboard at current position
        if (!clipboard.is_empty()) {
            int curLine = wksp.topline + cursor_line;
            int curCol  = wksp.offset + cursor_col;
            paste(curLine, curCol);
        }
        return;
    }
    // ^D - Delete character at cursor
    if (ch == 4) { // Ctrl-D
        if (lines.empty())
            lines.push_back("");
        int curLine = wksp.topline + cursor_line;
        if (curLine >= 0 && curLine < (int)lines.size()) {
            std::string &ln = lines[curLine];
            if (cursor_col < (int)ln.size()) {
                ln.erase((size_t)cursor_col, 1);
                build_segment_chain_from_lines();
                ensure_cursor_visible();
            } else if (curLine + 1 < (int)lines.size()) {
                // Join with next line
                ln += lines[curLine + 1];
                lines.erase(lines.begin() + curLine + 1);
                build_segment_chain_from_lines();
                ensure_cursor_visible();
            }
        }
        return;
    }
    // ^Y - Delete current line
    if (ch == 25) { // Ctrl-Y
        int curLine = wksp.topline + cursor_line;
        if (curLine >= 0 && curLine < (int)lines.size()) {
            clipboard.clear();
            clipboard.lines.push_back(lines[curLine]);
            clipboard.start_line = curLine;
            clipboard.end_line   = curLine;
            lines.erase(lines.begin() + curLine);
            if (lines.empty())
                lines.push_back("");
            if (cursor_line >= (int)lines.size()) {
                cursor_line = (int)lines.size() - 1;
                if (cursor_line < 0)
                    cursor_line = 0;
            }
            build_segment_chain_from_lines();
            ensure_cursor_visible();
        }
        return;
    }
    // ^C - Copy current line to clipboard
    if (ch == 3) { // Ctrl-C
        int curLine = wksp.topline + cursor_line;
        clipboard.clear();
        if (curLine >= 0 && curLine < (int)lines.size()) {
            clipboard.lines.push_back(lines[curLine]);
            clipboard.start_line = curLine;
            clipboard.end_line   = curLine;
            status               = "Copied line";
        }
        return;
    }
    // ^V - Paste clipboard at current position
    if (ch == 22) { // Ctrl-V
        if (!clipboard.is_empty()) {
            int curLine = wksp.topline + cursor_line;
            int curCol  = wksp.offset + cursor_col;
            paste(curLine, curCol);
        }
        return;
    }
    // ^O - Insert blank line
    if (ch == 15) { // Ctrl-O
        int curLine = wksp.topline + cursor_line;
        lines.insert(lines.begin() + curLine + 1, std::string());
        build_segment_chain_from_lines();
        ensure_cursor_visible();
        return;
    }
    // ^P - Quote next character
    if (ch == 16) { // Ctrl-P
        quote_next = true;
        return;
    }
    // ^F - Search forward
    if (ch == 6) { // Ctrl-F
        cmd_mode = true;
        cmd      = "/";
        status   = std::string("Cmd: ") + cmd;
        return;
    }
    // ^B - Search backward
    if (ch == 2) { // Ctrl-B
        cmd_mode = true;
        cmd      = "?";
        status   = std::string("Cmd: ") + cmd;
        return;
    }
    // F7 - Search forward
    if (ch == KEY_F(7)) {
        cmd_mode = true;
        cmd      = "/";
        status   = std::string("Cmd: ") + cmd;
        return;
    }
    // ^X - Prefix for extended commands
    if (ch == 24) { // Ctrl-X
        ctrlx_state = true;
        return;
    }
    // ^X f - Shift view right
    if (ctrlx_state && (ch == 'f' || ch == 'F')) {
        int shift = param_count > 0 ? param_count : ncols / 4;
        wksp.offset += shift;
        param_count = 0;
        ctrlx_state = false;
        ensure_cursor_visible();
        return;
    }
    // ^X b - Shift view left
    if (ctrlx_state && (ch == 'b' || ch == 'B')) {
        int shift = param_count > 0 ? param_count : ncols / 4;
        wksp.offset -= shift;
        if (wksp.offset < 0)
            wksp.offset = 0;
        param_count = 0;
        ctrlx_state = false;
        ensure_cursor_visible();
        return;
    }
    // ^X i - Toggle insert/overwrite mode
    if (ctrlx_state && (ch == 'i' || ch == 'I')) {
        insert_mode = !insert_mode;
        status      = std::string("Mode: ") + (insert_mode ? "INSERT" : "OVERWRITE");
        ctrlx_state = false;
        return;
    }
    // ^X ^C - Exit with saving
    if (ctrlx_state && (ch == 3 || ch == 'c' || ch == 'C')) {
        save_file();
        quit_flag   = true;
        status      = "Saved and exiting";
        ctrlx_state = false;
        return;
    }
    if (ctrlx_state) {
        ctrlx_state = false; // Clear state on any other key
    }
    // F8 - Goto line dialog
    if (ch == KEY_F(8)) {
        cmd_mode = true;
        cmd      = "g";
        status   = std::string("Cmd: ") + cmd;
        return;
    }

    // ^N - Switch to alternative workspace
    if (ch == 14) { // Ctrl+N
        switch_to_alternative_workspace();
        return;
    }
    if (ch == KEY_LEFT) {
        move_left();
        return;
    }
    if (ch == KEY_RIGHT) {
        move_right();
        return;
    }
    if (ch == KEY_UP) {
        move_up();
        return;
    }
    if (ch == KEY_DOWN) {
        move_down();
        return;
    }

    if (ch == KEY_HOME) {
        wksp.offset = 0;
        cursor_col  = 0;
        return;
    }
    if (ch == KEY_END) {
        int len = current_line_length();
        if (len >= ncols - 1) {
            wksp.offset = len - (ncols - 2);
            if (wksp.offset < 0)
                wksp.offset = 0;
            cursor_col = len - wksp.offset;
            if (cursor_col > ncols - 2)
                cursor_col = ncols - 2;
        } else {
            wksp.offset = 0;
            cursor_col  = len;
        }
        return;
    }
    if (ch == KEY_NPAGE) {
        int total = !files.empty() && files[wksp.wfile].chain ? files[wksp.wfile].nlines
                                                              : (int)lines.size();
        int step  = nlines - 2;
        if (step < 1)
            step = 1;
        wksp.topline += step;
        if (wksp.topline > std::max(0, total - (nlines - 1)))
            wksp.topline = std::max(0, total - (nlines - 1));
        ensure_cursor_visible();
        return;
    }
    if (ch == KEY_PPAGE) {
        int step = nlines - 2;
        if (step < 1)
            step = 1;
        wksp.topline -= step;
        if (wksp.topline < 0)
            wksp.topline = 0;
        ensure_cursor_visible();
        return;
    }
    if (ch == 12 /* Ctrl-L */) {
        // Force full redraw
        wksp_redraw();
        return;
    }
    if (ch == KEY_RESIZE) {
        ncols  = COLS;
        nlines = LINES;
        ensure_cursor_visible();
        return;
    }

    // --- Basic editing operations on the in-memory lines, then rebuild model ---
    if (lines.empty())
        lines.push_back("");
    int curLine = cursor_line + wksp.topline;
    if (curLine < 0)
        curLine = 0;
    if (curLine >= (int)lines.size())
        curLine = (int)lines.size() - 1;

    std::string &ln = lines[curLine];

    if (ch == KEY_BACKSPACE || ch == 127) {
        if (cursor_col > 0) {
            if (cursor_col <= (int)ln.size()) {
                ln.erase((size_t)cursor_col - 1, 1);
                cursor_col--;
            }
        } else if (curLine > 0) {
            int prevLen = (int)lines[curLine - 1].size();
            lines[curLine - 1] += ln;
            lines.erase(lines.begin() + curLine);
            cursor_line = cursor_line > 0 ? cursor_line - 1 : 0;
            cursor_col  = prevLen;
        }
        build_segment_chain_from_lines();
        ensure_cursor_visible();
        return;
    }

    if (ch == KEY_DC) {
        if (cursor_col < (int)ln.size()) {
            ln.erase((size_t)cursor_col, 1);
        } else if (curLine + 1 < (int)lines.size()) {
            ln += lines[curLine + 1];
            lines.erase(lines.begin() + curLine + 1);
        }
        build_segment_chain_from_lines();
        ensure_cursor_visible();
        return;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
        std::string tail;
        if (cursor_col < (int)ln.size()) {
            tail = ln.substr((size_t)cursor_col);
            ln.erase((size_t)cursor_col);
        }
        lines.insert(lines.begin() + curLine + 1, tail);
        if (cursor_line + 1 < nlines - 1) {
            cursor_line++;
        } else {
            // At bottom of viewport: scroll down
            wksp.topline++;
            // keep cursor on last content row
            cursor_line = nlines - 2;
        }
        cursor_col = 0;
        build_segment_chain_from_lines();
        ensure_cursor_visible();
        return;
    }

    if (ch == '\t') {
        ln.insert((size_t)cursor_col, 4, ' ');
        cursor_col += 4;
        build_segment_chain_from_lines();
        ensure_cursor_visible();
        return;
    }

    if (ch >= 32 && ch < 127) {
        if (quote_next) {
            // Quote mode: insert character literally (including control chars shown as ^X)
            char quoted = (char)ch;
            if (ch < 32) {
                quoted = (char)(ch + 64); // Convert to ^A, ^B, etc.
            }
            ln.insert((size_t)cursor_col, 1, quoted);
            cursor_col++;
            quote_next = false;
        } else if (insert_mode) {
            ln.insert((size_t)cursor_col, 1, (char)ch);
            cursor_col++;
        } else {
            // Overwrite mode
            if (cursor_col < (int)ln.size()) {
                ln[(size_t)cursor_col] = (char)ch;
            } else {
                ln.push_back((char)ch);
            }
            cursor_col++;
        }
        segments_dirty = true; // mark segments as needing rebuild
        ensure_cursor_visible();
        return;
    }
}
