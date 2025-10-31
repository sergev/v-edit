#include <signal.h>

#include "editor.h"

//
// Navigate to specified line number.
//
void Editor::goto_line(int line_number)
{
    auto total = wksp_->total_line_count();

    if (line_number < 0)
        line_number = 0;
    if (line_number >= total)
        line_number = total - 1;
    if (line_number < 0)
        line_number = 0;

    wksp_->view.topline = line_number;
    cursor_line_        = 0;
    cursor_col_         = 0;
    wksp_->view.basecol = 0;
    ensure_cursor_visible();
}

//
// Backend editing method: Handle backspace operation.
//
void Editor::edit_backspace()
{
    int cur_line = wksp_->view.topline + cursor_line_;
    if (cur_line < 0)
        cur_line = 0;
    get_line(cur_line);

    size_t actual_col = get_actual_col();
    if (actual_col > 0) {
        if (actual_col <= current_line_.size()) {
            current_line_.erase(actual_col - 1, 1);
            current_line_modified_ = true;
            cursor_col_--;
        }
    } else if (cur_line > 0) {
        // Join with previous line
        get_line(cur_line - 1);
        std::string prev = current_line_;
        get_line(cur_line);
        prev += current_line_;
        current_line_          = prev;
        current_line_no_       = cur_line - 1;
        current_line_modified_ = true;
        put_line();
        // Delete current line
        wksp_->delete_contents(cur_line, cur_line);
        cursor_line_ = cursor_line_ > 0 ? cursor_line_ - 1 : 0;
        cursor_col_  = prev.size();
    }
    put_line();
    ensure_cursor_visible();
}

//
// Backend editing method: Handle delete operation.
//
void Editor::edit_delete()
{
    int cur_line = wksp_->view.topline + cursor_line_;
    if (cur_line < 0)
        cur_line = 0;
    get_line(cur_line);

    size_t actual_col = get_actual_col();
    if (actual_col < current_line_.size()) {
        current_line_.erase(actual_col, 1);
        current_line_modified_ = true;
    } else if (cur_line + 1 < wksp_->total_line_count()) {
        // Join with next line
        // Preserve current line before loading next
        std::string curr = current_line_;
        get_line(cur_line + 1);
        std::string next = current_line_;
        curr += next;
        current_line_          = curr;
        current_line_no_       = cur_line;
        current_line_modified_ = true;
        put_line();
        wksp_->delete_contents(cur_line + 1, cur_line + 1);
        // Place cursor at the join point (end of original current line)
        cursor_col_ = (int)actual_col;
    }
    put_line();
    ensure_cursor_visible();
}

//
// Backend editing method: Handle enter/newline operation.
//
void Editor::edit_enter()
{
    int cur_line = wksp_->view.topline + cursor_line_;
    if (cur_line < 0)
        cur_line = 0;
    get_line(cur_line);

    size_t actual_col = get_actual_col();
    std::string tail;
    if (actual_col < current_line_.size()) {
        tail = current_line_.substr(actual_col);
        current_line_.erase(actual_col);
    }
    current_line_modified_ = true;
    put_line();

    // Insert new line with tail content
    if (!tail.empty()) {
        auto temp_segments = tempfile_.write_line_to_temp(tail);
        if (!temp_segments.empty()) {
            wksp_->insert_contents(temp_segments, cur_line + 1);
        } else {
            auto blank = wksp_->create_blank_lines(1);
            wksp_->insert_contents(blank, cur_line + 1);
        }
    } else {
        auto blank = wksp_->create_blank_lines(1);
        wksp_->insert_contents(blank, cur_line + 1);
    }

    // Invalidate current_line_no_ since we've moved to a different line
    current_line_no_ = -1;

    if (cursor_line_ + 1 < nlines_ - 1) {
        cursor_line_++;
    } else {
        wksp_->view.topline = wksp_->view.topline + 1;
        cursor_line_        = nlines_ - 2;
    }
    cursor_col_ = 0;
    ensure_cursor_visible();
}

//
// Backend editing method: Handle tab insertion.
//
void Editor::edit_tab()
{
    int cur_line = wksp_->view.topline + cursor_line_;
    if (cur_line < 0)
        cur_line = 0;
    get_line(cur_line);

    size_t actual_col = get_actual_col();
    current_line_.insert(actual_col, 4, ' ');
    cursor_col_ += 4;
    current_line_modified_ = true;
    put_line();
    ensure_cursor_visible();
}

//
// Backend editing method: Handle character insertion/overwrite.
//
void Editor::edit_insert_char(char ch)
{
    int cur_line = wksp_->view.topline + cursor_line_;
    if (cur_line < 0)
        cur_line = 0;
    get_line(cur_line);

    size_t actual_col = get_actual_col();
    // Pad with spaces if inserting at virtual column position beyond line end
    if (actual_col > current_line_.size()) {
        current_line_.resize(actual_col, ' ');
    }
    if (quote_next_) {
        // Quote mode: insert character literally
        char quoted = ch;
        if (ch < 32) {
            quoted = (char)(ch + 64); // Convert to ^A, ^B, etc.
        }
        current_line_.insert(actual_col, 1, quoted);
        cursor_col_++;
        quote_next_ = false;
    } else if (insert_mode_) {
        current_line_.insert(actual_col, 1, ch);
        cursor_col_++;
    } else {
        // Overwrite mode
        if (actual_col < current_line_.size()) {
            current_line_[actual_col] = ch;
        } else {
            current_line_.push_back(ch);
        }
        cursor_col_++;
    }
    current_line_modified_ = true;
    put_line();
    ensure_cursor_visible();
}

//
// Move cursor one position left.
//
void Editor::move_left()
{
    if (cursor_col_ > 0) {
        cursor_col_--;
    } else if (wksp_->view.basecol > 0) {
        wksp_->view.basecol = wksp_->view.basecol - 1;
    } else if (cursor_line_ > 0) {
        cursor_line_--;
        int len     = current_line_length();
        cursor_col_ = len;
        if (cursor_col_ >= ncols_ - 1) {
            wksp_->view.basecol = len - (ncols_ - 2);
            cursor_col_         = ncols_ - 2;
        }
    }
}

//
// Move cursor one position right.
//
void Editor::move_right()
{
    if (cursor_col_ < ncols_ - 1) {
        cursor_col_++;
    } else {
        wksp_->view.basecol = wksp_->view.basecol + 1;
    }
}

//
// Move cursor one line up.
//
void Editor::move_up()
{
    if (cursor_line_ > 0) {
        cursor_line_--;
    } else if (wksp_->view.topline > 0) {
        wksp_->view.topline = wksp_->view.topline - 1;
    }
    ensure_cursor_visible();
}

//
// Move cursor one line down.
//
void Editor::move_down()
{
    if (cursor_line_ < nlines_ - 2) {
        cursor_line_++;
    } else {
        wksp_->view.topline = wksp_->view.topline + 1;
    }
    ensure_cursor_visible();
}

//
// Return length of current line in characters.
//
int Editor::current_line_length() const
{
    int cur_line = wksp_->view.topline + cursor_line_;
    if (cur_line < 0 || cur_line >= wksp_->total_line_count()) {
        return 0;
    }
    return (int)wksp_->read_line(cur_line).size();
}

//
// Get actual column position in line (accounts for horizontal scrolling)
//
size_t Editor::get_actual_col() const
{
    return static_cast<size_t>(wksp_->view.basecol + cursor_col_);
}

//
// Search forward for text pattern.
//
bool Editor::search_forward(const std::string &needle)
{
    int start_line = wksp_->view.topline + cursor_line_;
    int start_col  = wksp_->view.basecol + cursor_col_;
    auto total     = wksp_->total_line_count();

    // Search from current position forward
    for (int i = start_line; i < total; ++i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = (i == start_line) ? (size_t)start_col : 0;
        pos              = line.find(needle, pos);
        if (pos != std::string::npos) {
            // Found it - position cursor
            wksp_->view.topline = i;
            cursor_line_        = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (int)pos - (ncols_ - 10);
            } else {
                wksp_->view.basecol = 0;
            }
            cursor_col_ = (int)pos - wksp_->view.basecol;
            ensure_cursor_visible();
            status_ = std::string("Found: ") + needle;
            return true;
        }
    }

    // Wrap around to beginning
    for (int i = 0; i <= start_line; ++i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = 0;
        if (i == start_line) {
            pos = line.find(needle, (size_t)start_col);
            if (pos == std::string::npos)
                continue;
        } else {
            pos = line.find(needle);
        }
        if (pos != std::string::npos) {
            wksp_->view.topline = i;
            cursor_line_        = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (int)pos - (ncols_ - 10);
            } else {
                wksp_->view.basecol = 0;
            }
            cursor_col_ = (int)pos - wksp_->view.basecol;
            ensure_cursor_visible();
            status_ = std::string("Found: ") + needle;
            return true;
        }
    }

    status_ = std::string("Not found: ") + needle;
    return false;
}

//
// Search backward for text pattern.
//
bool Editor::search_backward(const std::string &needle)
{
    int start_line = wksp_->view.topline + cursor_line_;
    int start_col  = wksp_->view.basecol + cursor_col_;
    auto total     = wksp_->total_line_count();

    // Search from current position backward
    for (int i = start_line; i >= 0; --i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = std::string::npos;
        if (i == start_line) {
            pos = line.rfind(needle, (size_t)start_col);
        } else {
            pos = line.rfind(needle);
        }
        if (pos != std::string::npos) {
            wksp_->view.topline = i;
            cursor_line_        = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (int)pos - (ncols_ - 10);
            } else {
                wksp_->view.basecol = 0;
            }
            cursor_col_ = (int)pos - wksp_->view.basecol;
            ensure_cursor_visible();
            status_ = std::string("Found: ") + needle;
            return true;
        }
    }

    // Wrap around to end
    for (int i = total - 1; i > start_line; --i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = line.rfind(needle);
        if (pos != std::string::npos) {
            wksp_->view.topline = i;
            cursor_line_        = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (int)pos - (ncols_ - 10);
            } else {
                wksp_->view.basecol = 0;
            }
            cursor_col_ = (int)pos - wksp_->view.basecol;
            ensure_cursor_visible();
            status_ = std::string("Found: ") + needle;
            return true;
        }
    }

    status_ = std::string("Not found: ") + needle;
    return false;
}

//
// Repeat last search in same direction.
//
bool Editor::search_next()
{
    if (!last_search_.empty()) {
        if (last_search_forward_) {
            return search_forward(last_search_);
        } else {
            return search_backward(last_search_);
        }
    }
    return false;
}

//
// Repeat last search in opposite direction.
//
bool Editor::search_prev()
{
    if (!last_search_.empty()) {
        if (last_search_forward_) {
            return search_backward(last_search_);
        } else {
            return search_forward(last_search_);
        }
    }
    return false;
}

// Core line operations matching prototype behavior

//
// Insert blank lines at specified position.
//
void Editor::insertlines(int from, int number)
{
    if (from < 0 || number < 1)
        return;

    put_line();

    // Insert blank lines using workspace segments
    auto blank = wksp_->create_blank_lines(number);
    wksp_->insert_contents(blank, from);

    ensure_cursor_visible();
}

//
// Delete lines starting at specified position.
//
void Editor::deletelines(int from, int number)
{
    if (from < 0 || number < 1)
        return;

    put_line();

    // Save to clipboard (delete buffer)
    picklines(from, number);

    // Delete the lines using workspace segments
    wksp_->delete_contents(from, from + number - 1);

    ensure_cursor_visible();
}

//
// Split line into two at cursor position.
//
void Editor::splitline(int line, int col)
{
    if (line < 0 || col < 0)
        return;

    put_line();

    // Get the line using current_line_ buffer
    get_line(line);
    std::string ln = current_line_;

    // Split at column
    if (col >= (int)ln.size()) {
        // Just insert a blank line after
        insertlines(line + 1, 1);
        return;
    }

    std::string tail = ln.substr(col);
    ln.erase(col);

    // Update current line
    current_line_          = ln;
    current_line_no_       = line;
    current_line_modified_ = true;
    put_line();

    // Insert new line with tail content
    auto temp_segments = tempfile_.write_line_to_temp(tail);
    if (!temp_segments.empty()) {
        // Move the segment into the workspace segments list
        wksp_->insert_contents(temp_segments, line + 1);
    } else {
        // Fallback: insert blank line
        insertlines(line + 1, 1);
    }

    ensure_cursor_visible();
}

//
// Combine current line with next line at cursor position.
//
void Editor::combineline(int line, int col)
{
    if (line < 0 || col < 0)
        return;

    auto total = wksp_->total_line_count();
    if (line + 1 >= total)
        return; // No next line to combine

    put_line();

    // Get both lines
    get_line(line);
    std::string current = current_line_;
    get_line(line + 1);
    std::string next = current_line_;

    // Combine at column
    // Pad current line to col if needed
    if (col > (int)current.size()) {
        current.resize(col, ' ');
    }

    current += next;

    // Update line
    current_line_          = current;
    current_line_no_       = line;
    current_line_modified_ = true;
    put_line();

    // Delete the next line
    wksp_->delete_contents(line + 1, line + 1);

    ensure_cursor_visible();
}

//
// Command mode operation helpers (moved from key_bindings.cpp)
//

//
// Parse numeric count from command string.
//
int Editor::parse_count_from_cmd(const std::string &cmd, int default_count)
{
    if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
        // Extract all leading digits
        size_t i = 0;
        while (i < cmd.size() && cmd[i] >= '0' && cmd[i] <= '9') {
            i++;
        }
        if (i > 0) {
            int count = std::atoi(cmd.substr(0, i).c_str());
            return count < 1 ? default_count : count;
        }
    }
    return default_count;
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

//
// Exit command mode and clear related state.
//
void Editor::exit_command_mode(bool clear_area_selection, bool clear_filter)
{
    cmd_mode_ = false;
    if (clear_area_selection) {
        area_selection_mode_ = false;
        params_.type         = Parameters::PARAM_NONE;
    }
    if (clear_filter) {
        filter_mode_ = false;
    }
    cmd_.clear();
    params_.reset();
}

//
// Handle copy lines command.
//
void Editor::handle_copy_lines_cmd(int count)
{
    int cur_line = wksp_->view.topline + cursor_line_;
    picklines(cur_line, count);
    status_ = std::string("Copied ") + std::to_string(count) + " line(s)";
    exit_command_mode(true, true);
}

//
// Handle delete lines command.
//
void Editor::handle_delete_lines_cmd(int count)
{
    int cur_line = wksp_->view.topline + cursor_line_;
    deletelines(cur_line, count);
    status_ = std::string("Deleted ") + std::to_string(count) + " line(s)";
    exit_command_mode(true, true);
}

//
// Handle insert lines command.
//
void Editor::handle_insert_lines_cmd(int count)
{
    int cur_line = wksp_->view.topline + cursor_line_;
    insertlines(cur_line, count);
    status_ = std::string("Inserted ") + std::to_string(count) + " line(s)";
    exit_command_mode(true, true);
}

//
// Start area selection if movement key detected.
//
void Editor::start_area_selection_if_movement(int ch)
{
    if (is_movement_key(ch)) {
        if (!area_selection_mode_) {
            // Start area selection
            area_selection_mode_ = true;
            int cur_col          = wksp_->view.basecol + cursor_col_;
            int cur_row          = wksp_->view.topline + cursor_line_;
            params_.c0           = cur_col;
            params_.r0           = cur_row;
            params_.c1           = cur_col;
            params_.r1           = cur_row;
            status_              = "*** Area defined by cursor ***";
        }
        handle_area_selection(ch);
    }
}

//
// Execute command string.
//
void Editor::execute_command(const std::string &cmd)
{
    if (cmd.empty()) {
        return;
    }

    // Parse numeric count if command starts with a number
    std::string remaining_cmd = cmd;
    if (!cmd.empty() && cmd[0] >= '0' && cmd[0] <= '9') {
        size_t i = 0;
        while (i < cmd.size() && cmd[i] >= '0' && cmd[i] <= '9') {
            i++;
        }
        if (i > 0) {
            params_.count = std::atoi(cmd.substr(0, i).c_str());
            remaining_cmd = cmd.substr(i);
        }
    }

    if (remaining_cmd == "qa") {
        quit_flag_ = true;
        status_    = "Exiting without saving changes";
    } else if (remaining_cmd == "ad") {
        // Force a crash dump
        handle_fatal_signal(SIGQUIT);
    } else if (remaining_cmd == "q") {
        save_file();
        quit_flag_ = true;
        status_    = "Saving changes and exiting";
    } else if (remaining_cmd == "r") {
        // Redraw screen
        wksp_redraw();
        status_ = "Redrawn";
    } else if (remaining_cmd.size() >= 2 && remaining_cmd.substr(0, 2) == "w ") {
        // w + to make writable (or other w commands)
        if (remaining_cmd.size() >= 3 && remaining_cmd[2] == '+') {
            wksp_->file_state.writable = true;
            status_                    = "File marked writable";
        } else {
            wksp_->file_state.writable = false;
            status_                    = "File marked read-only";
        }
    } else if (remaining_cmd == "s") {
        save_file();
    } else if (remaining_cmd.size() > 1 && remaining_cmd[0] == 's' && remaining_cmd[1] != ' ') {
        // s<filename> - save as
        std::string new_filename = remaining_cmd.substr(1);
        save_as(new_filename);
    } else if (remaining_cmd.size() > 1 && remaining_cmd[0] == 'o') {
        // Open file: o<filename> - now opens in current workspace
        std::string file_to_open = remaining_cmd.substr(1);
        filename_                = file_to_open;
        if (load_file_segments(file_to_open)) {
            status_ = std::string("Opened: ") + file_to_open;
        } else {
            status_ = std::string("Failed to open: ") + file_to_open;
        }
    } else if (filter_mode_ && !remaining_cmd.empty()) {
        // External filter command
        int cur_line  = wksp_->view.topline + cursor_line_;
        int num_lines = 1; // default to current line

        // Parse command for line count (e.g., "3 sort" means sort 3 lines)
        std::string command = remaining_cmd;
        size_t spacePos     = remaining_cmd.find(' ');
        if (spacePos != std::string::npos) {
            std::string countStr = remaining_cmd.substr(0, spacePos);
            if (countStr.find_first_not_of("0123456789") == std::string::npos) {
                num_lines = std::atoi(countStr.c_str());
                if (num_lines < 1)
                    num_lines = 1;
                command = remaining_cmd.substr(spacePos + 1);
                // Trim leading spaces from command
                while (!command.empty() && command[0] == ' ') {
                    command = command.substr(1);
                }
            }
        } else {
            // No space found, check if command starts with a number
            if (!remaining_cmd.empty() && remaining_cmd[0] >= '0' && remaining_cmd[0] <= '9') {
                // Extract number from beginning
                size_t i = 0;
                while (i < remaining_cmd.size() && remaining_cmd[i] >= '0' &&
                       remaining_cmd[i] <= '9') {
                    i++;
                }
                if (i > 0) {
                    std::string countStr = remaining_cmd.substr(0, i);
                    num_lines            = std::atoi(countStr.c_str());
                    if (num_lines < 1)
                        num_lines = 1;
                    command = remaining_cmd.substr(i);
                }
            }
        }

        // Debug: show what we're executing
        status_ =
            std::string("Executing: ") + command + " on " + std::to_string(num_lines) + " lines";

        if (execute_external_filter(command, cur_line, num_lines)) {
            status_ = std::string("Filtered ") + std::to_string(num_lines) + " line(s)";
        } else {
            status_ = "Filter execution failed";
        }
        filter_mode_ = false;
    } else if (remaining_cmd.size() > 1 && remaining_cmd[0] == 'g') {
        // goto line: g<number>
        int ln = std::atoi(remaining_cmd.c_str() + 1);
        if (ln < 1)
            ln = 1;
        goto_line(ln - 1);
    } else if (remaining_cmd.size() > 1 && remaining_cmd[0] == '/') {
        // search: /text
        std::string needle = remaining_cmd.substr(1);
        if (!needle.empty()) {
            last_search_forward_ = true;
            last_search_         = needle;
            search_forward(needle);
        }
    } else if (remaining_cmd.size() > 1 && remaining_cmd[0] == '?') {
        // backward search: ?text
        std::string needle = remaining_cmd.substr(1);
        if (!needle.empty()) {
            last_search_forward_ = false;
            last_search_         = needle;
            search_backward(needle);
        }
    } else if (remaining_cmd == "n") {
        if (last_search_forward_)
            search_next();
        else
            search_prev();
    } else if (remaining_cmd.size() == 2 && remaining_cmd[0] == '>' && remaining_cmd[1] >= 'a' &&
               remaining_cmd[1] <= 'z') {
        // Save macro buffer: >x (saves current clipboard to named buffer)
        save_macro_buffer(remaining_cmd[1]);
        status_ = std::string("Buffer '") + remaining_cmd[1] + "' saved";
    } else if (remaining_cmd.size() == 3 && remaining_cmd[0] == '>' && remaining_cmd[1] == '>' &&
               remaining_cmd[2] >= 'a' && remaining_cmd[2] <= 'z') {
        // Save macro position: >>x (saves current position)
        save_macro_position(remaining_cmd[2]);
        status_ = std::string("Position '") + remaining_cmd[2] + "' saved";
    } else if (remaining_cmd.size() == 2 && remaining_cmd[0] == '$' && remaining_cmd[1] >= 'a' &&
               remaining_cmd[1] <= 'z') {
        // Use macro: $x (tries buffer first, then position)
        // Check if in area selection mode for mdeftag
        if (area_selection_mode_) {
            // Define area using tag
            mdeftag(remaining_cmd[1]);
            // Stay in area selection mode for further commands
            return;
        }
        auto it = macros_.find(remaining_cmd[1]);
        if (it != macros_.end()) {
            if (it->second.is_buffer()) {
                if (paste_macro_buffer(remaining_cmd[1])) {
                    status_ = std::string("Pasted buffer '") + remaining_cmd[1] + "'";
                } else {
                    status_ = std::string("Buffer '") + remaining_cmd[1] + "' empty";
                }
            } else if (it->second.is_position()) {
                if (goto_macro_position(remaining_cmd[1])) {
                    status_ = std::string("Goto position '") + remaining_cmd[1] + "'";
                } else {
                    status_ = std::string("Position '") + remaining_cmd[1] + "' not found";
                }
            }
        } else {
            status_ = std::string("Macro '") + remaining_cmd[1] + "' not found";
        }
    } else if (remaining_cmd.size() >= 1 && remaining_cmd[0] >= '0' && remaining_cmd[0] <= '9') {
        // Direct line number - goto line
        int ln = std::atoi(remaining_cmd.c_str());
        if (ln >= 1) {
            goto_line(ln - 1);
            status_ = std::string("Goto line ") + remaining_cmd;
        }
    }
}
