#include <ncurses.h>

#include <iostream>

#include "editor.h"

//
// Route key to appropriate handler based on mode.
//
void Editor::handle_key(int ch)
{
    if (cmd_mode_) {
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
    if (area_selection_mode_) {
        if (ch == 3) { // ^C - Copy rectangular block
            // Finalize area bounds
            if (param_c0_ > param_c1_) {
                std::swap(param_c0_, param_c1_);
            }
            if (param_r0_ > param_r1_) {
                std::swap(param_r0_, param_r1_);
            }
            int numCols  = param_c1_ - param_c0_ + 1;
            int numLines = param_r1_ - param_r0_ + 1;
            pickspaces(param_r0_, param_c0_, numCols, numLines);

            // Check if we should store to a named buffer (>name)
            if (!cmd_.empty() && cmd_[0] == '>' && cmd_.size() == 2 && cmd_[1] >= 'a' &&
                cmd_[1] <= 'z') {
                save_macro_buffer(cmd_[1]);
                status_ = std::string("Copied and saved to buffer '") + cmd_[1] + "'";
            } else {
                status_ = "Copied rectangular block";
            }

            cmd_mode_            = false;
            area_selection_mode_ = false;
            cmd_.clear();
            param_type_ = 0;
            return;
        }
        if (ch == 25) { // ^Y - Delete rectangular block
            // Finalize area bounds
            if (param_c0_ > param_c1_) {
                std::swap(param_c0_, param_c1_);
            }
            if (param_r0_ > param_r1_) {
                std::swap(param_r0_, param_r1_);
            }
            int numCols  = param_c1_ - param_c0_ + 1;
            int numLines = param_r1_ - param_r0_ + 1;
            closespaces(param_r0_, param_c0_, numCols, numLines);

            // Check if we should store to a named buffer (>name)
            if (!cmd_.empty() && cmd_[0] == '>' && cmd_.size() == 2 && cmd_[1] >= 'a' &&
                cmd_[1] <= 'z') {
                save_macro_buffer(cmd_[1]);
                status_ = std::string("Deleted and saved to buffer '") + cmd_[1] + "'";
            } else {
                status_ = "Deleted rectangular block";
            }

            cmd_mode_            = false;
            area_selection_mode_ = false;
            cmd_.clear();
            param_type_ = 0;
            return;
        }
        if (ch == 15) { // ^O - Insert rectangular block of spaces
            // Finalize area bounds
            if (param_c0_ > param_c1_) {
                std::swap(param_c0_, param_c1_);
            }
            if (param_r0_ > param_r1_) {
                std::swap(param_r0_, param_r1_);
            }
            int numCols  = param_c1_ - param_c0_ + 1;
            int numLines = param_r1_ - param_r0_ + 1;
            openspaces(param_r0_, param_c0_, numCols, numLines);
            status_              = "Inserted rectangular spaces";
            cmd_mode_            = false;
            area_selection_mode_ = false;
            cmd_.clear();
            param_type_ = 0;
            return;
        }
    }

    // Handle control characters in command mode (not in area selection)
    if (ch == 3) { // ^C - Copy lines
        int curLine = wksp_->topline() + cursor_line_;
        // Parse count from cmd_ if it's numeric
        int count = 1;
        if (!cmd_.empty() && cmd_[0] >= '0' && cmd_[0] <= '9') {
            // Extract all leading digits
            size_t i = 0;
            while (i < cmd_.size() && cmd_[i] >= '0' && cmd_[i] <= '9') {
                i++;
            }
            if (i > 0) {
                count = std::atoi(cmd_.substr(0, i).c_str());
                if (count < 1)
                    count = 1;
            }
        }
        picklines(curLine, count);
        status_   = std::string("Copied ") + std::to_string(count) + " line(s)";
        cmd_mode_ = false;
        cmd_.clear();
        return;
    }
    if (ch == 25) { // ^Y - Delete lines
        int curLine = wksp_->topline() + cursor_line_;
        // Parse count from cmd_ if it's numeric
        int count = 1;
        if (!cmd_.empty() && cmd_[0] >= '0' && cmd_[0] <= '9') {
            count = std::atoi(cmd_.c_str());
            if (count < 1)
                count = 1;
        }
        deletelines(curLine, count);
        status_   = std::string("Deleted ") + std::to_string(count) + " line(s)";
        cmd_mode_ = false;
        cmd_.clear();
        return;
    }
    if (ch == 15) { // ^O - Insert blank lines
        int curLine = wksp_->topline() + cursor_line_;
        // Parse count from cmd_ if it's numeric
        int count = 1;
        if (!cmd_.empty() && cmd_[0] >= '0' && cmd_[0] <= '9') {
            count = std::atoi(cmd_.c_str());
            if (count < 1)
                count = 1;
        }
        insertlines(curLine, count);
        status_   = std::string("Inserted ") + std::to_string(count) + " line(s)";
        cmd_mode_ = false;
        cmd_.clear();
        return;
    }

    // Check if this is a movement key (area selection)
    if (is_movement_key(ch)) {
        if (!area_selection_mode_) {
            // Start area selection
            area_selection_mode_ = true;
            param_c0_ = param_c1_ = wksp_->basecol() + cursor_col_;
            param_r0_ = param_r1_ = wksp_->topline() + cursor_line_;
            status_               = "*** Area defined by cursor ***";
        }
        handle_area_selection(ch);
        return;
    }

    // Handle Enter key in area selection - it just moves cursor (start a new line)
    if ((ch == '\n' || ch == KEY_ENTER) && area_selection_mode_) {
        // Enter just moves the cursor, doesn't finalize
        move_down();
        handle_area_selection(ch);
        return;
    }

    if (ch == 27 || ch == KEY_F(1) || ch == 1) {
        // ESC or F1 or ^A cancels area selection
        if (area_selection_mode_) {
            area_selection_mode_ = false;
            param_type_          = 0;
            cmd_mode_            = false;
            status_              = "Cancelled";
            return;
        }
    }

    if (ch == 27) {
        cmd_.clear();
        filter_mode_         = false;
        area_selection_mode_ = false;
        param_type_          = 0;
        return;
    } // ESC
    if (ch == '\n' || ch == KEY_ENTER) {
        if (area_selection_mode_) {
            // This shouldn't happen anymore as we handle Enter above
            return;
        }
        // Parse numeric count if command starts with a number
        if (!cmd_.empty() && cmd_[0] >= '0' && cmd_[0] <= '9') {
            size_t i = 0;
            while (i < cmd_.size() && cmd_[i] >= '0' && cmd_[i] <= '9') {
                i++;
            }
            if (i > 0) {
                param_count_ = std::atoi(cmd_.substr(0, i).c_str());
                cmd_         = cmd_.substr(i);
            }
        }
        if (!cmd_.empty()) {
            if (cmd_ == "qa") {
                quit_flag_ = true;
                status_    = "Exiting";
            } else if (cmd_ == "ad") {
                quit_flag_ = true;
                status_    = "ABORTED";
            } else if (cmd_ == "q") {
                quit_flag_ = true;
                status_    = "Exiting";
            } else if (cmd_ == "r") {
                // Redraw screen
                wksp_redraw();
                status_ = "Redrawn";
            } else if (cmd_.size() >= 2 && cmd_.substr(0, 2) == "w ") {
                // w + to make writable (or other w commands)
                if (cmd_.size() >= 3 && cmd_[2] == '+') {
                    wksp_->set_writable(1);
                    status_ = "File marked writable";
                } else {
                    wksp_->set_writable(0);
                    status_ = "File marked read-only";
                }
            } else if (cmd_ == "s") {
                save_file();
            } else if (cmd_.size() > 1 && cmd_[0] == 's' && cmd_[1] != ' ') {
                // s<filename_> - save as
                std::string new_filename_ = cmd_.substr(1);
                save_as(new_filename_);
            } else if (cmd_.size() > 1 && cmd_[0] == 'o') {
                // Open file: o<filename_> - now opens in current workspace
                std::string file_to_open = cmd_.substr(1);
                filename_                = file_to_open;
                if (load_file_segments(file_to_open)) {
                    status_ = std::string("Opened: ") + file_to_open;
                } else {
                    status_ = std::string("Failed to open: ") + file_to_open;
                }
            } else if (filter_mode_ && !cmd_.empty()) {
                // External filter command
                int curLine  = wksp_->topline() + cursor_line_;
                int numLines = 1; // default to current line

                // Parse command for line count (e.g., "3 sort" means sort 3 lines)
                std::string command = cmd_;
                size_t spacePos     = cmd_.find(' ');
                if (spacePos != std::string::npos) {
                    std::string countStr = cmd_.substr(0, spacePos);
                    if (countStr.find_first_not_of("0123456789") == std::string::npos) {
                        numLines = std::atoi(countStr.c_str());
                        if (numLines < 1)
                            numLines = 1;
                        command = cmd_.substr(spacePos + 1);
                        // Trim leading spaces from command
                        while (!command.empty() && command[0] == ' ') {
                            command = command.substr(1);
                        }
                    }
                } else {
                    // No space found, check if command starts with a number
                    if (!cmd_.empty() && cmd_[0] >= '0' && cmd_[0] <= '9') {
                        // Extract number from beginning
                        size_t i = 0;
                        while (i < cmd_.size() && cmd_[i] >= '0' && cmd_[i] <= '9') {
                            i++;
                        }
                        if (i > 0) {
                            std::string countStr = cmd_.substr(0, i);
                            numLines             = std::atoi(countStr.c_str());
                            if (numLines < 1)
                                numLines = 1;
                            command = cmd_.substr(i);
                        }
                    }
                }

                // Debug: show what we're executing
                status_ = std::string("Executing: ") + command + " on " + std::to_string(numLines) +
                          " lines";

                if (execute_external_filter(command, curLine, numLines)) {
                    status_ = std::string("Filtered ") + std::to_string(numLines) + " line(s)";
                } else {
                    status_ = "Filter execution failed";
                }
                filter_mode_ = false;
            } else if (cmd_.size() > 1 && cmd_[0] == 'g') {
                // goto line: g<number>
                int ln = std::atoi(cmd_.c_str() + 1);
                if (ln < 1)
                    ln = 1;
                goto_line(ln - 1);
            } else if (cmd_.size() > 1 && cmd_[0] == '/') {
                // search: /text
                std::string needle = cmd_.substr(1);
                if (!needle.empty()) {
                    last_search_forward_ = true;
                    last_search_         = needle;
                    search_forward(needle);
                }
            } else if (cmd_.size() > 1 && cmd_[0] == '?') {
                // backward search: ?text
                std::string needle = cmd_.substr(1);
                if (!needle.empty()) {
                    last_search_forward_ = false;
                    last_search_         = needle;
                    search_backward(needle);
                }
            } else if (cmd_ == "n") {
                if (last_search_forward_)
                    search_next();
                else
                    search_prev();
            } else if (cmd_.size() == 2 && cmd_[0] == '>' && cmd_[1] >= 'a' && cmd_[1] <= 'z') {
                // Save macro buffer: >x (saves current clipboard_ to named buffer)
                save_macro_buffer(cmd_[1]);
                status_ = std::string("Buffer '") + cmd_[1] + "' saved";
            } else if (cmd_.size() == 3 && cmd_[0] == '>' && cmd_[1] == '>' && cmd_[2] >= 'a' &&
                       cmd_[2] <= 'z') {
                // Save macro position: >>x (saves current position)
                save_macro_position(cmd_[2]);
                status_ = std::string("Position '") + cmd_[2] + "' saved";
            } else if (cmd_.size() == 2 && cmd_[0] == '$' && cmd_[1] >= 'a' && cmd_[1] <= 'z') {
                // Use macro: $x (tries buffer first, then position)
                // Check if in area selection mode for mdeftag
                if (area_selection_mode_) {
                    // Define area using tag
                    mdeftag(cmd_[1]);
                    // Stay in area selection mode for further commands
                    return;
                }
                auto it = macros_.find(cmd_[1]);
                if (it != macros_.end()) {
                    if (it->second.isBuffer()) {
                        if (paste_macro_buffer(cmd_[1])) {
                            status_ = std::string("Pasted buffer '") + cmd_[1] + "'";
                        } else {
                            status_ = std::string("Buffer '") + cmd_[1] + "' empty";
                        }
                    } else if (it->second.isPosition()) {
                        if (goto_macro_position(cmd_[1])) {
                            status_ = std::string("Goto position '") + cmd_[1] + "'";
                        } else {
                            status_ = std::string("Position '") + cmd_[1] + "' not found";
                        }
                    }
                } else {
                    status_ = std::string("Macro '") + cmd_[1] + "' not found";
                }
            } else if (cmd_.size() >= 1 && cmd_[0] == ':') {
                // Colon commands: :w, :q, :wq, :<number>
                if (cmd_ == ":w") {
                    save_file();
                } else if (cmd_ == ":q") {
                    quit_flag_ = true;
                    status_    = "Exiting";
                } else if (cmd_ == ":wq") {
                    save_file();
                    quit_flag_ = true;
                    status_    = "Saved and exiting";
                } else if (cmd_.size() > 1 && cmd_[0] == ':') {
                    // :<number> - goto line
                    int ln = std::atoi(cmd_.c_str() + 1);
                    if (ln >= 1) {
                        goto_line(ln - 1);
                        status_ = std::string("Goto line ") + std::to_string(ln);
                    }
                }
            } else if (cmd_.size() >= 1 && cmd_[0] >= '0' && cmd_[0] <= '9') {
                // Direct line number - goto line
                int ln = std::atoi(cmd_.c_str());
                if (ln >= 1) {
                    goto_line(ln - 1);
                    status_ = std::string("Goto line ") + cmd_;
                }
            }
        }
        // Clear param_count_ at end of command
        param_count_         = 0;
        cmd_mode_            = false;
        filter_mode_         = false;
        area_selection_mode_ = false;
        cmd_.clear();
        param_type_ = 0;
        return;
    }

    if (ch == KEY_BACKSPACE || ch == 127) {
        if (!cmd_.empty())
            cmd_.pop_back();
        return;
    }

    // Handle control characters immediately, even in command mode
    if (ch == 3) { // ^C - Copy
        if (cmd_mode_ && !area_selection_mode_ && !filter_mode_) {
            int curLine = wksp_->topline() + cursor_line_;
            int count   = param_count_ > 0 ? param_count_ : 1;
            picklines(curLine, count);
            status_      = std::string("Copied ") + std::to_string(count) + " line(s)";
            cmd_mode_    = false;
            param_count_ = 0;
            cmd_.clear();
            return;
        }
    }
    if (ch == 25) { // ^Y - Delete
        if (cmd_mode_ && !area_selection_mode_ && !filter_mode_) {
            int curLine = wksp_->topline() + cursor_line_;
            int count   = param_count_ > 0 ? param_count_ : 1;
            deletelines(curLine, count);
            status_      = std::string("Deleted ") + std::to_string(count) + " line(s)";
            cmd_mode_    = false;
            param_count_ = 0;
            cmd_.clear();
            return;
        }
    }
    if (ch == 15) { // ^O - Insert blank lines
        if (cmd_mode_ && !area_selection_mode_ && !filter_mode_) {
            int curLine = wksp_->topline() + cursor_line_;
            int count   = param_count_ > 0 ? param_count_ : 1;
            insertlines(curLine, count);
            status_      = std::string("Inserted ") + std::to_string(count) + " line(s)";
            cmd_mode_    = false;
            param_count_ = 0;
            cmd_.clear();
            return;
        }
    }

    if (ch >= 32 && ch < 127) {
        cmd_.push_back((char)ch);
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
        cursor_col_ = 0;
        break;
    case KEY_END: {
        int curLine = wksp_->topline() + cursor_line_;
        get_line(curLine);
        cursor_col_ = current_line_.length();
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
    int curCol  = wksp_->basecol() + cursor_col_;
    int curLine = wksp_->topline() + cursor_line_;

    if (curCol > param_c0_) {
        param_c1_ = curCol;
    } else {
        param_c0_ = curCol;
    }

    if (curLine > param_r0_) {
        param_r1_ = curLine;
    } else {
        param_r0_ = curLine;
    }
}

//
// Switch to command input mode.
//
void Editor::enter_command_mode()
{
    cmd_mode_            = true;
    area_selection_mode_ = false;
    param_type_          = 0;
    param_count_         = 0;
    param_str_.clear();
    cmd_.clear();
    status_ = "Cmd: ";
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
        cmd_mode_    = true;
        filter_mode_ = true;
        cmd_         = "";
        status_      = "Filter command: ";
        return;
    }
    if (ch == KEY_F(5)) {
        // Copy current line to clipboard_
        int curLine = wksp_->topline() + cursor_line_;
        picklines(curLine, 1);
        status_ = "Copied";
        return;
    }
    if (ch == KEY_F(6)) {
        // Paste clipboard_ at current position
        if (!clipboard_.is_empty()) {
            int curLine = wksp_->topline() + cursor_line_;
            int curCol  = wksp_->basecol() + cursor_col_;
            paste(curLine, curCol);
        }
        return;
    }
    // ^D - Delete character at cursor
    if (ch == 4) { // Ctrl-D
        int curLine = wksp_->topline() + cursor_line_;
        get_line(curLine);
        if (cursor_col_ < (int)current_line_.size()) {
            current_line_.erase((size_t)cursor_col_, 1);
            current_line_modified_ = true;
            put_line();
            ensure_cursor_visible();
        } else if (curLine + 1 < wksp_->nlines()) {
            // Join with next line
            get_line(curLine + 1);
            current_line_ += current_line_;
            current_line_no_       = curLine;
            current_line_modified_ = true;
            put_line();
            // Delete next line
            wksp_->delete_segments(curLine + 1, curLine + 1);
            ensure_cursor_visible();
        }
        return;
    }
    // ^Y - Delete current line
    if (ch == 25) { // Ctrl-Y
        int curLine = wksp_->topline() + cursor_line_;
        if (curLine >= 0 && curLine < wksp_->nlines()) {
            picklines(curLine, 1); // Copy to clipboard_ before deleting
            wksp_->delete_segments(curLine, curLine);
            wksp_->set_nlines(wksp_->nlines() - 1);
            if (cursor_line_ >= wksp_->nlines() - 1) {
                cursor_line_ = wksp_->nlines() - 2;
                if (cursor_line_ < 0)
                    cursor_line_ = 0;
            }
            ensure_cursor_visible();
        }
        return;
    }
    // ^C - Copy current line to clipboard_
    if (ch == 3) { // Ctrl-C
        int curLine = wksp_->topline() + cursor_line_;
        picklines(curLine, 1);
        status_ = "Copied line";
        return;
    }
    // ^V - Paste clipboard_ at current position
    if (ch == 22) { // Ctrl-V
        if (!clipboard_.is_empty()) {
            int curLine = wksp_->topline() + cursor_line_;
            int curCol  = wksp_->basecol() + cursor_col_;
            paste(curLine, curCol);
        }
        return;
    }
    // ^O - Insert blank line
    if (ch == 15) { // Ctrl-O
        int curLine    = wksp_->topline() + cursor_line_;
        Segment *blank = wksp_->create_blank_lines(1);
        wksp_->insert_segments(blank, curLine + 1);
        wksp_->set_nlines(wksp_->nlines() + 1);
        ensure_cursor_visible();
        return;
    }
    // ^P - Quote next character
    if (ch == 16) { // Ctrl-P
        quote_next_ = true;
        return;
    }
    // ^F - Search forward
    if (ch == 6) { // Ctrl-F
        cmd_mode_ = true;
        cmd_      = "/";
        status_   = std::string("Cmd: ") + cmd_;
        return;
    }
    // ^B - Search backward
    if (ch == 2) { // Ctrl-B
        cmd_mode_ = true;
        cmd_      = "?";
        status_   = std::string("Cmd: ") + cmd_;
        return;
    }
    // F7 - Search forward
    if (ch == KEY_F(7)) {
        cmd_mode_ = true;
        cmd_      = "/";
        status_   = std::string("Cmd: ") + cmd_;
        return;
    }
    // ^X - Prefix for extended commands
    if (ch == 24) { // Ctrl-X
        ctrlx_state_ = true;
        return;
    }
    // ^X f - Shift view right
    if (ctrlx_state_ && (ch == 'f' || ch == 'F')) {
        int shift = param_count_ > 0 ? param_count_ : ncols_ / 4;
        wksp_->set_basecol(wksp_->basecol() + shift);
        param_count_ = 0;
        ctrlx_state_ = false;
        ensure_cursor_visible();
        return;
    }
    // ^X b - Shift view left
    if (ctrlx_state_ && (ch == 'b' || ch == 'B')) {
        int shift = param_count_ > 0 ? param_count_ : ncols_ / 4;
        wksp_->set_basecol(wksp_->basecol() - shift);
        if (wksp_->basecol() < 0)
            wksp_->set_basecol(0);
        param_count_ = 0;
        ctrlx_state_ = false;
        ensure_cursor_visible();
        return;
    }
    // ^X i - Toggle insert/overwrite mode
    if (ctrlx_state_ && (ch == 'i' || ch == 'I')) {
        insert_mode_ = !insert_mode_;
        status_      = std::string("Mode: ") + (insert_mode_ ? "INSERT" : "OVERWRITE");
        ctrlx_state_ = false;
        return;
    }
    // ^X ^C - Exit with saving
    if (ctrlx_state_ && (ch == 3 || ch == 'c' || ch == 'C')) {
        save_file();
        quit_flag_   = true;
        status_      = "Saved and exiting";
        ctrlx_state_ = false;
        return;
    }
    if (ctrlx_state_) {
        ctrlx_state_ = false; // Clear state on any other key
    }
    // F8 - Goto line dialog
    if (ch == KEY_F(8)) {
        cmd_mode_ = true;
        cmd_      = "g";
        status_   = std::string("Cmd: ") + cmd_;
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
        wksp_->set_basecol(0);
        cursor_col_ = 0;
        return;
    }
    if (ch == KEY_END) {
        int len = current_line_length();
        if (len >= ncols_ - 1) {
            wksp_->set_basecol(len - (ncols_ - 2));
            if (wksp_->basecol() < 0)
                wksp_->set_basecol(0);
            cursor_col_ = len - wksp_->basecol();
            if (cursor_col_ > ncols_ - 2)
                cursor_col_ = ncols_ - 2;
        } else {
            wksp_->set_basecol(0);
            cursor_col_ = len;
        }
        return;
    }
    if (ch == KEY_NPAGE) {
        int total = wksp_->nlines();
        int step  = nlines_ - 2;
        if (step < 1)
            step = 1;
        wksp_->set_topline(wksp_->topline() + step);
        if (wksp_->topline() > std::max(0, total - (nlines_ - 1)))
            wksp_->set_topline(std::max(0, total - (nlines_ - 1)));
        ensure_cursor_visible();
        return;
    }
    if (ch == KEY_PPAGE) {
        int step = nlines_ - 2;
        if (step < 1)
            step = 1;
        wksp_->set_topline(wksp_->topline() - step);
        if (wksp_->topline() < 0)
            wksp_->set_topline(0);
        ensure_cursor_visible();
        return;
    }
    if (ch == 12 /* Ctrl-L */) {
        // Force full redraw
        wksp_redraw();
        return;
    }
    if (ch == KEY_RESIZE) {
        ncols_  = COLS;
        nlines_ = LINES;
        ensure_cursor_visible();
        return;
    }

    // --- Basic editing operations using current_line_ buffer ---
    int curLine = wksp_->topline() + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

    if (ch == KEY_BACKSPACE || ch == 127) {
        if (cursor_col_ > 0) {
            if (cursor_col_ <= (int)current_line_.size()) {
                current_line_.erase((size_t)cursor_col_ - 1, 1);
                current_line_modified_ = true;
                cursor_col_--;
            }
        } else if (curLine > 0) {
            // Join with previous line
            get_line(curLine - 1);
            std::string prev = current_line_;
            get_line(curLine);
            prev += current_line_;
            current_line_          = prev;
            current_line_no_       = curLine - 1;
            current_line_modified_ = true;
            put_line();
            // Delete current line
            wksp_->delete_segments(curLine, curLine);
            cursor_line_ = cursor_line_ > 0 ? cursor_line_ - 1 : 0;
            cursor_col_  = prev.size();
        }
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch == KEY_DC) {
        if (cursor_col_ < (int)current_line_.size()) {
            current_line_.erase((size_t)cursor_col_, 1);
            current_line_modified_ = true;
        } else if (curLine + 1 < wksp_->nlines()) {
            // Join with next line
            get_line(curLine + 1);
            current_line_ += current_line_;
            current_line_no_       = curLine;
            current_line_modified_ = true;
            put_line();
            wksp_->delete_segments(curLine + 1, curLine + 1);
        }
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
        std::string tail;
        if (cursor_col_ < (int)current_line_.size()) {
            tail = current_line_.substr((size_t)cursor_col_);
            current_line_.erase((size_t)cursor_col_);
        }
        current_line_modified_ = true;
        put_line();
        // Insert new line with tail content
        if (!tail.empty()) {
            Segment *tail_seg = tempfile_.write_line_to_temp(tail);
            if (tail_seg) {
                wksp_->insert_segments(tail_seg, curLine + 1);
            } else {
                // Fallback: insert blank line
                Segment *blank = wksp_->create_blank_lines(1);
                wksp_->insert_segments(blank, curLine + 1);
            }
        } else {
            Segment *blank = wksp_->create_blank_lines(1);
            wksp_->insert_segments(blank, curLine + 1);
        }
        if (cursor_line_ + 1 < nlines_ - 1) {
            cursor_line_++;
        } else {
            // At bottom of viewport: scroll down
            wksp_->set_topline(wksp_->topline() + 1);
            // keep cursor on last content row
            cursor_line_ = nlines_ - 2;
        }
        cursor_col_ = 0;
        wksp_->set_nlines(wksp_->nlines() + 1);
        ensure_cursor_visible();
        return;
    }

    if (ch == '\t') {
        current_line_.insert((size_t)cursor_col_, 4, ' ');
        cursor_col_ += 4;
        current_line_modified_ = true;
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch >= 32 && ch < 127) {
        if (quote_next_) {
            // Quote mode: insert character literally (including control chars shown as ^X)
            char quoted = (char)ch;
            if (ch < 32) {
                quoted = (char)(ch + 64); // Convert to ^A, ^B, etc.
            }
            current_line_.insert((size_t)cursor_col_, 1, quoted);
            cursor_col_++;
            quote_next_ = false;
        } else if (insert_mode_) {
            current_line_.insert((size_t)cursor_col_, 1, (char)ch);
            cursor_col_++;
        } else {
            // Overwrite mode
            if (cursor_col_ < (int)current_line_.size()) {
                current_line_[(size_t)cursor_col_] = (char)ch;
            } else {
                current_line_.push_back((char)ch);
            }
            cursor_col_++;
        }
        current_line_modified_ = true;
        put_line();
        ensure_cursor_visible();
        return;
    }
}
