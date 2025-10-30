#include <ncurses.h>

#include <iostream>

#include "editor.h"

//
// Process key input in command mode.
//
void Editor::handle_key_cmd(int ch)
{
    // Handle rectangular block operations if area is being selected
    if (area_selection_mode_) {
        if (ch == 3) { // ^C - Copy rectangular block
            // Finalize area bounds
            params_.normalize_area();
            int c0, r0, c1, r1;
            params_.get_area_start(c0, r0);
            params_.get_area_end(c1, r1);
            int numCols  = c1 - c0 + 1;
            int numLines = r1 - r0 + 1;
            pickspaces(r0, c0, numCols, numLines);

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
            params_.set_type(Parameters::PARAM_NONE);
            return;
        }
        if (ch == 25) { // ^Y - Delete rectangular block
            // Finalize area bounds
            params_.normalize_area();
            int c0, r0, c1, r1;
            params_.get_area_start(c0, r0);
            params_.get_area_end(c1, r1);
            int num_cols  = c1 - c0 + 1;
            int num_lines = r1 - r0 + 1;
            closespaces(r0, c0, num_cols, num_lines);

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
            params_.set_type(Parameters::PARAM_NONE);
            return;
        }
        if (ch == 15) { // ^O - Insert rectangular block of spaces
            // Finalize area bounds
            params_.normalize_area();
            int c0, r0, c1, r1;
            params_.get_area_start(c0, r0);
            params_.get_area_end(c1, r1);
            int num_cols  = c1 - c0 + 1;
            int num_lines = r1 - r0 + 1;
            openspaces(r0, c0, num_cols, num_lines);
            status_              = "Inserted rectangular spaces";
            cmd_mode_            = false;
            area_selection_mode_ = false;
            cmd_.clear();
            params_.set_type(Parameters::PARAM_NONE);
            return;
        }
    }

    // Handle control characters in command mode (not in area selection)
    if (ch == 3) { // ^C - Copy lines
        int curLine = wksp_->view.topline + cursor_line_;
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
        int curLine = wksp_->view.topline + cursor_line_;
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
        int curLine = wksp_->view.topline + cursor_line_;
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
            int cur_col          = wksp_->view.basecol + cursor_col_;
            int cur_row          = wksp_->view.topline + cursor_line_;
            params_.set_area_start(cur_col, cur_row);
            params_.set_area_end(cur_col, cur_row);
            status_ = "*** Area defined by cursor ***";
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
        cmd_mode_ = false;
        if (area_selection_mode_) {
            area_selection_mode_ = false;
            params_.set_type(Parameters::PARAM_NONE);
            status_ = "Cancelled";
        }
        return;
    }

    if (ch == 27) {
        cmd_.clear();
        filter_mode_         = false;
        area_selection_mode_ = false;
        params_.set_type(Parameters::PARAM_NONE);
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
                params_.set_count(std::atoi(cmd_.substr(0, i).c_str()));
                cmd_ = cmd_.substr(i);
            }
        }
        if (!cmd_.empty()) {
            if (cmd_ == "qa") {
                quit_flag_ = true;
                status_    = "Exiting without saving changes";
            } else if (cmd_ == "ad") {
                // Force a crash dump
                handle_fatal_signal(SIGQUIT);
            } else if (cmd_ == "q") {
                save_file();
                quit_flag_ = true;
                status_    = "Saving changes and exiting";
            } else if (cmd_ == "r") {
                // Redraw screen
                wksp_redraw();
                status_ = "Redrawn";
            } else if (cmd_.size() >= 2 && cmd_.substr(0, 2) == "w ") {
                // w + to make writable (or other w commands)
                if (cmd_.size() >= 3 && cmd_[2] == '+') {
                    wksp_->file_state.writable = true;
                    status_                    = "File marked writable";
                } else {
                    wksp_->file_state.writable = false;
                    status_                    = "File marked read-only";
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
                int curLine  = wksp_->view.topline + cursor_line_;
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
        // Clear parameters at end of command
        params_.reset();
        cmd_mode_            = false;
        filter_mode_         = false;
        area_selection_mode_ = false;
        cmd_.clear();
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
            int curLine = wksp_->view.topline + cursor_line_;
            int count   = params_.get_count() > 0 ? params_.get_count() : 1;
            picklines(curLine, count);
            status_   = std::string("Copied ") + std::to_string(count) + " line(s)";
            cmd_mode_ = false;
            params_.set_count(0);
            cmd_.clear();
            return;
        }
    }
    if (ch == 25) { // ^Y - Delete
        if (cmd_mode_ && !area_selection_mode_ && !filter_mode_) {
            int curLine = wksp_->view.topline + cursor_line_;
            int count   = params_.get_count() > 0 ? params_.get_count() : 1;
            deletelines(curLine, count);
            status_   = std::string("Deleted ") + std::to_string(count) + " line(s)";
            cmd_mode_ = false;
            params_.set_count(0);
            cmd_.clear();
            return;
        }
    }
    if (ch == 15) { // ^O - Insert blank lines
        if (cmd_mode_ && !area_selection_mode_ && !filter_mode_) {
            int curLine = wksp_->view.topline + cursor_line_;
            int count   = params_.get_count() > 0 ? params_.get_count() : 1;
            insertlines(curLine, count);
            status_   = std::string("Inserted ") + std::to_string(count) + " line(s)";
            cmd_mode_ = false;
            params_.set_count(0);
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
        int curLine = wksp_->view.topline + cursor_line_;
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
    int curCol  = wksp_->view.basecol + cursor_col_;
    int curLine = wksp_->view.topline + cursor_line_;

    // Get current area bounds
    int c0, r0, c1, r1;
    params_.get_area_start(c0, r0);
    params_.get_area_end(c1, r1);

    if (curCol > c0) {
        params_.set_area_end(curCol, r1);
    } else {
        params_.set_area_start(curCol, r0);
    }

    if (curLine > r0) {
        int c;
        params_.get_area_end(c, r1);
        params_.set_area_end(c, curLine);
    } else {
        auto blank = wksp_->create_blank_lines(1);
        wksp_->insert_contents(blank, curLine + 1);
    }
}

//
// Switch to command input mode.
//
void Editor::enter_command_mode()
{
    cmd_mode_            = true;
    area_selection_mode_ = false;
    params_.reset();
    cmd_.clear();
    status_ = "Cmd: ";
}
