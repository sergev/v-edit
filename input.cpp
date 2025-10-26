#include <ncurses.h>

#include <iostream>

#include "editor.h"

//
// Route key to appropriate handler based on mode.
//
void Editor::handle_key(int ch)
{
    if (cmd_mode) {
        handle_key_cmd(ch);
        return;
    }
    handle_key_edit(ch);
}

//
// Process key input in command mode.
//
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
            if (!cmd.empty() && cmd[0] == '>' && cmd.size() == 2 && cmd[1] >= 'a' &&
                cmd[1] <= 'z') {
                save_macro_buffer(cmd[1]);
                status = std::string("Copied and saved to buffer '") + cmd[1] + "'";
            } else {
                status = "Copied rectangular block";
            }

            cmd_mode            = false;
            area_selection_mode = false;
            cmd.clear();
            param_type = 0;
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
            if (!cmd.empty() && cmd[0] == '>' && cmd.size() == 2 && cmd[1] >= 'a' &&
                cmd[1] <= 'z') {
                save_macro_buffer(cmd[1]);
                status = std::string("Deleted and saved to buffer '") + cmd[1] + "'";
            } else {
                status = "Deleted rectangular block";
            }

            cmd_mode            = false;
            area_selection_mode = false;
            cmd.clear();
            param_type = 0;
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
            param_type = 0;
            return;
        }
    }

    // Handle control characters in command mode (not in area selection)
    if (ch == 3) { // ^C - Copy lines
        int curLine = wksp->topline() + cursor_line;
        // Parse count from cmd if it's numeric
        int count = 1;
        if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
            // Extract all leading digits
            size_t i = 0;
            while (i < cmd.size() && cmd[i] >= '0' && cmd[i] <= '9') {
                i++;
            }
            if (i > 0) {
                count = std::atoi(cmd.substr(0, i).c_str());
                if (count < 1)
                    count = 1;
            }
        }
        picklines(curLine, count);
        status   = std::string("Copied ") + std::to_string(count) + " line(s)";
        cmd_mode = false;
        cmd.clear();
        return;
    }
    if (ch == 25) { // ^Y - Delete lines
        int curLine = wksp->topline() + cursor_line;
        // Parse count from cmd if it's numeric
        int count = 1;
        if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
            count = std::atoi(cmd.c_str());
            if (count < 1)
                count = 1;
        }
        deletelines(curLine, count);
        status   = std::string("Deleted ") + std::to_string(count) + " line(s)";
        cmd_mode = false;
        cmd.clear();
        return;
    }
    if (ch == 15) { // ^O - Insert blank lines
        int curLine = wksp->topline() + cursor_line;
        // Parse count from cmd if it's numeric
        int count = 1;
        if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
            count = std::atoi(cmd.c_str());
            if (count < 1)
                count = 1;
        }
        insertlines(curLine, count);
        status   = std::string("Inserted ") + std::to_string(count) + " line(s)";
        cmd_mode = false;
        cmd.clear();
        return;
    }

    // Check if this is a movement key (area selection)
    if (is_movement_key(ch)) {
        if (!area_selection_mode) {
            // Start area selection
            area_selection_mode = true;
            param_c0 = param_c1 = wksp->basecol() + cursor_col;
            param_r0 = param_r1 = wksp->topline() + cursor_line;
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
                cmd         = cmd.substr(i);
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
                    wksp->set_writable(1);
                    status = "File marked writable";
                } else {
                    wksp->set_writable(0);
                    status = "File marked read-only";
                }
            } else if (cmd == "s") {
                save_file();
            } else if (cmd.size() > 1 && cmd[0] == 's' && cmd[1] != ' ') {
                // s<filename> - save as
                std::string new_filename = cmd.substr(1);
                save_as(new_filename);
            } else if (cmd.size() > 1 && cmd[0] == 'o') {
                // Open file: o<filename> - now opens in current workspace
                std::string file_to_open = cmd.substr(1);
                filename                 = file_to_open;
                if (load_file_segments(file_to_open)) {
                    status = std::string("Opened: ") + file_to_open;
                } else {
                    status = std::string("Failed to open: ") + file_to_open;
                }
            } else if (filter_mode && !cmd.empty()) {
                // External filter command
                int curLine  = wksp->topline() + cursor_line;
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
                    if (it->second.isBuffer()) {
                        if (paste_macro_buffer(cmd[1])) {
                            status = std::string("Pasted buffer '") + cmd[1] + "'";
                        } else {
                            status = std::string("Buffer '") + cmd[1] + "' empty";
                        }
                    } else if (it->second.isPosition()) {
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
        param_count         = 0;
        cmd_mode            = false;
        filter_mode         = false;
        area_selection_mode = false;
        cmd.clear();
        param_type = 0;
        return;
    }

    if (ch == KEY_BACKSPACE || ch == 127) {
        if (!cmd.empty())
            cmd.pop_back();
        return;
    }

    // Handle control characters immediately, even in command mode
    if (ch == 3) { // ^C - Copy
        if (cmd_mode && !area_selection_mode && !filter_mode) {
            int curLine = wksp->topline() + cursor_line;
            int count   = param_count > 0 ? param_count : 1;
            picklines(curLine, count);
            status      = std::string("Copied ") + std::to_string(count) + " line(s)";
            cmd_mode    = false;
            param_count = 0;
            cmd.clear();
            return;
        }
    }
    if (ch == 25) { // ^Y - Delete
        if (cmd_mode && !area_selection_mode && !filter_mode) {
            int curLine = wksp->topline() + cursor_line;
            int count   = param_count > 0 ? param_count : 1;
            deletelines(curLine, count);
            status      = std::string("Deleted ") + std::to_string(count) + " line(s)";
            cmd_mode    = false;
            param_count = 0;
            cmd.clear();
            return;
        }
    }
    if (ch == 15) { // ^O - Insert blank lines
        if (cmd_mode && !area_selection_mode && !filter_mode) {
            int curLine = wksp->topline() + cursor_line;
            int count   = param_count > 0 ? param_count : 1;
            insertlines(curLine, count);
            status      = std::string("Inserted ") + std::to_string(count) + " line(s)";
            cmd_mode    = false;
            param_count = 0;
            cmd.clear();
            return;
        }
    }

    if (ch >= 32 && ch < 127) {
        cmd.push_back((char)ch);
    }
}

// Enhanced parameter system functions
//
// Check if key is a cursor movement key.
//
bool Editor::is_movement_key(int ch) const
{
    return (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN || ch == KEY_HOME ||
            ch == KEY_END || ch == KEY_PPAGE || ch == KEY_NPAGE) &&
           ch != '\n' && ch != '\r' && ch != KEY_ENTER;
}

//
// Handle cursor movement during area selection.
//
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
    case KEY_END: {
        int curLine = wksp->topline() + cursor_line;
        get_line(curLine);
        cursor_col = current_line.length();
        break;
    }
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
    int curCol  = wksp->basecol() + cursor_col;
    int curLine = wksp->topline() + cursor_line;

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

//
// Switch to command input mode.
//
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

//
// Process key input in edit mode.
//
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
        // F3 - Switch to alternative workspace
        switch_to_alternative_workspace();
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
        int curLine = wksp->topline() + cursor_line;
        picklines(curLine, 1);
        status = "Copied";
        return;
    }
    if (ch == KEY_F(6)) {
        // Paste clipboard at current position
        if (!clipboard.is_empty()) {
            int curLine = wksp->topline() + cursor_line;
            int curCol  = wksp->basecol() + cursor_col;
            paste(curLine, curCol);
        }
        return;
    }
    // ^D - Delete character at cursor
    if (ch == 4) { // Ctrl-D
        int curLine = wksp->topline() + cursor_line;
        get_line(curLine);
        if (cursor_col < (int)current_line.size()) {
            current_line.erase((size_t)cursor_col, 1);
            current_line_modified = true;
            put_line();
            ensure_cursor_visible();
        } else if (curLine + 1 < wksp->nlines()) {
            // Join with next line
            get_line(curLine + 1);
            current_line += current_line;
            current_line_no       = curLine;
            current_line_modified = true;
            put_line();
            // Delete next line
            wksp->delete_segments(curLine + 1, curLine + 1);
            ensure_cursor_visible();
        }
        return;
    }
    // ^Y - Delete current line
    if (ch == 25) { // Ctrl-Y
        int curLine = wksp->topline() + cursor_line;
        if (curLine >= 0 && curLine < wksp->nlines()) {
            picklines(curLine, 1); // Copy to clipboard before deleting
            wksp->delete_segments(curLine, curLine);
            wksp->set_nlines(wksp->nlines() - 1);
            if (cursor_line >= wksp->nlines() - 1) {
                cursor_line = wksp->nlines() - 2;
                if (cursor_line < 0)
                    cursor_line = 0;
            }
            ensure_cursor_visible();
        }
        return;
    }
    // ^C - Copy current line to clipboard
    if (ch == 3) { // Ctrl-C
        int curLine = wksp->topline() + cursor_line;
        picklines(curLine, 1);
        status = "Copied line";
        return;
    }
    // ^V - Paste clipboard at current position
    if (ch == 22) { // Ctrl-V
        if (!clipboard.is_empty()) {
            int curLine = wksp->topline() + cursor_line;
            int curCol  = wksp->basecol() + cursor_col;
            paste(curLine, curCol);
        }
        return;
    }
    // ^O - Insert blank line
    if (ch == 15) { // Ctrl-O
        int curLine    = wksp->topline() + cursor_line;
        Segment *blank = wksp->create_blank_lines(1);
        wksp->insert_segments(blank, curLine + 1);
        wksp->set_nlines(wksp->nlines() + 1);
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
        wksp->set_basecol(wksp->basecol() + shift);
        param_count = 0;
        ctrlx_state = false;
        ensure_cursor_visible();
        return;
    }
    // ^X b - Shift view left
    if (ctrlx_state && (ch == 'b' || ch == 'B')) {
        int shift = param_count > 0 ? param_count : ncols / 4;
        wksp->set_basecol(wksp->basecol() - shift);
        if (wksp->basecol() < 0)
            wksp->set_basecol(0);
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
        wksp->set_basecol(0);
        cursor_col = 0;
        return;
    }
    if (ch == KEY_END) {
        int len = current_line_length();
        if (len >= ncols - 1) {
            wksp->set_basecol(len - (ncols - 2));
            if (wksp->basecol() < 0)
                wksp->set_basecol(0);
            cursor_col = len - wksp->basecol();
            if (cursor_col > ncols - 2)
                cursor_col = ncols - 2;
        } else {
            wksp->set_basecol(0);
            cursor_col = len;
        }
        return;
    }
    if (ch == KEY_NPAGE) {
        int total = wksp->nlines();
        int step  = nlines - 2;
        if (step < 1)
            step = 1;
        wksp->set_topline(wksp->topline() + step);
        if (wksp->topline() > std::max(0, total - (nlines - 1)))
            wksp->set_topline(std::max(0, total - (nlines - 1)));
        ensure_cursor_visible();
        return;
    }
    if (ch == KEY_PPAGE) {
        int step = nlines - 2;
        if (step < 1)
            step = 1;
        wksp->set_topline(wksp->topline() - step);
        if (wksp->topline() < 0)
            wksp->set_topline(0);
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

    // --- Basic editing operations using current_line buffer ---
    int curLine = wksp->topline() + cursor_line;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

    if (ch == KEY_BACKSPACE || ch == 127) {
        if (cursor_col > 0) {
            if (cursor_col <= (int)current_line.size()) {
                current_line.erase((size_t)cursor_col - 1, 1);
                current_line_modified = true;
                cursor_col--;
            }
        } else if (curLine > 0) {
            // Join with previous line
            get_line(curLine - 1);
            std::string prev = current_line;
            get_line(curLine);
            prev += current_line;
            current_line          = prev;
            current_line_no       = curLine - 1;
            current_line_modified = true;
            put_line();
            // Delete current line
            wksp->delete_segments(curLine, curLine);
            cursor_line = cursor_line > 0 ? cursor_line - 1 : 0;
            cursor_col  = prev.size();
        }
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch == KEY_DC) {
        if (cursor_col < (int)current_line.size()) {
            current_line.erase((size_t)cursor_col, 1);
            current_line_modified = true;
        } else if (curLine + 1 < wksp->nlines()) {
            // Join with next line
            get_line(curLine + 1);
            current_line += current_line;
            current_line_no       = curLine;
            current_line_modified = true;
            put_line();
            wksp->delete_segments(curLine + 1, curLine + 1);
        }
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
        std::string tail;
        if (cursor_col < (int)current_line.size()) {
            tail = current_line.substr((size_t)cursor_col);
            current_line.erase((size_t)cursor_col);
        }
        current_line_modified = true;
        put_line();
        // Insert new line with tail content
        if (!tail.empty()) {
            Segment *tail_seg = tempfile_.write_line_to_temp(tail);
            if (tail_seg) {
                wksp->insert_segments(tail_seg, curLine + 1);
            } else {
                // Fallback: insert blank line
                Segment *blank = wksp->create_blank_lines(1);
                wksp->insert_segments(blank, curLine + 1);
            }
        } else {
            Segment *blank = wksp->create_blank_lines(1);
            wksp->insert_segments(blank, curLine + 1);
        }
        if (cursor_line + 1 < nlines - 1) {
            cursor_line++;
        } else {
            // At bottom of viewport: scroll down
            wksp->set_topline(wksp->topline() + 1);
            // keep cursor on last content row
            cursor_line = nlines - 2;
        }
        cursor_col = 0;
        wksp->set_nlines(wksp->nlines() + 1);
        ensure_cursor_visible();
        return;
    }

    if (ch == '\t') {
        current_line.insert((size_t)cursor_col, 4, ' ');
        cursor_col += 4;
        current_line_modified = true;
        put_line();
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
            current_line.insert((size_t)cursor_col, 1, quoted);
            cursor_col++;
            quote_next = false;
        } else if (insert_mode) {
            current_line.insert((size_t)cursor_col, 1, (char)ch);
            cursor_col++;
        } else {
            // Overwrite mode
            if (cursor_col < (int)current_line.size()) {
                current_line[(size_t)cursor_col] = (char)ch;
            } else {
                current_line.push_back((char)ch);
            }
            cursor_col++;
        }
        current_line_modified = true;
        put_line();
        ensure_cursor_visible();
        return;
    }
}
