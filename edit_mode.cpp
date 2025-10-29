#include <ncurses.h>

#include "editor.h"

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
        int curLine = wksp_->view.topline + cursor_line_;
        picklines(curLine, 1);
        status_ = "Copied";
        return;
    }
    if (ch == KEY_F(6)) {
        // Paste clipboard_ at current position
        if (!clipboard_.is_empty()) {
            int curLine = wksp_->view.topline + cursor_line_;
            int curCol  = wksp_->view.basecol + cursor_col_;
            paste(curLine, curCol);
        }
        return;
    }
    // ^D - Delete character at cursor
    if (ch == 4) { // Ctrl-D
        int curLine = wksp_->view.topline + cursor_line_;
        get_line(curLine);
        if (cursor_col_ < (int)current_line_.size()) {
            current_line_.erase((size_t)cursor_col_, 1);
            current_line_modified_ = true;
            put_line();
            ensure_cursor_visible();
        } else if (curLine + 1 < wksp_->total_line_count()) {
            // Join with next line
            get_line(curLine + 1);
            current_line_ += current_line_;
            current_line_no_       = curLine;
            current_line_modified_ = true;
            put_line();
            // Delete next line
            wksp_->delete_contents(curLine + 1, curLine + 1);
            ensure_cursor_visible();
        }
        return;
    }
    // ^Y - Delete current line
    if (ch == 25) { // Ctrl-Y
        int curLine = wksp_->view.topline + cursor_line_;
        if (curLine >= 0 && curLine < wksp_->total_line_count()) {
            picklines(curLine, 1); // Copy to clipboard_ before deleting
            wksp_->delete_contents(curLine, curLine);
            auto total = wksp_->total_line_count();
            if (cursor_line_ >= total - 1) {
                cursor_line_ = total - 2;
                if (cursor_line_ < 0)
                    cursor_line_ = 0;
            }
            ensure_cursor_visible();
        }
        return;
    }
    // ^C - Copy current line to clipboard_
    if (ch == 3) { // Ctrl-C
        int curLine = wksp_->view.topline + cursor_line_;
        picklines(curLine, 1);
        status_ = "Copied line";
        return;
    }
    // ^V - Paste clipboard_ at current position
    if (ch == 22) { // Ctrl-V
        if (!clipboard_.is_empty()) {
            int curLine = wksp_->view.topline + cursor_line_;
            int curCol  = wksp_->view.basecol + cursor_col_;
            paste(curLine, curCol);
        }
        return;
    }
    // ^O - Insert blank line
    if (ch == 15) { // Ctrl-O
        int curLine = wksp_->view.topline + cursor_line_;
        auto blank  = wksp_->create_blank_lines(1);
        wksp_->insert_contents(blank, curLine + 1);
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
        int shift           = params_.get_count() > 0 ? params_.get_count() : ncols_ / 4;
        wksp_->view.basecol = wksp_->view.basecol + shift;
        params_.set_count(0);
        ctrlx_state_ = false;
        ensure_cursor_visible();
        return;
    }
    // ^X b - Shift view left
    if (ctrlx_state_ && (ch == 'b' || ch == 'B')) {
        int shift           = params_.get_count() > 0 ? params_.get_count() : ncols_ / 4;
        wksp_->view.basecol = wksp_->view.basecol - shift;
        if (wksp_->view.basecol < 0)
            wksp_->view.basecol = 0;
        params_.set_count(0);
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
        wksp_->view.basecol = 0;
        cursor_col_         = 0;
        return;
    }
    if (ch == KEY_END) {
        int len = current_line_length();
        if (len >= ncols_ - 1) {
            wksp_->view.basecol = len - (ncols_ - 2);
            if (wksp_->view.basecol < 0)
                wksp_->view.basecol = 0;
            cursor_col_ = len - wksp_->view.basecol;
            if (cursor_col_ > ncols_ - 2)
                cursor_col_ = ncols_ - 2;
        } else {
            wksp_->view.basecol = 0;
            cursor_col_         = len;
        }
        return;
    }
    if (ch == KEY_NPAGE) {
        auto total = wksp_->total_line_count();
        int step   = nlines_ - 2;
        if (step < 1)
            step = 1;
        wksp_->view.topline = wksp_->view.topline + step;
        if (wksp_->view.topline > std::max(0, total - (nlines_ - 1)))
            wksp_->view.topline = std::max(0, total - (nlines_ - 1));
        ensure_cursor_visible();
        return;
    }
    if (ch == KEY_PPAGE) {
        int step = nlines_ - 2;
        if (step < 1)
            step = 1;
        wksp_->view.topline = wksp_->view.topline - step;
        if (wksp_->view.topline < 0)
            wksp_->view.topline = 0;
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
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

    if (ch == KEY_BACKSPACE || ch == 127) {
        size_t actual_col = get_actual_col();
        if (actual_col > 0) {
            if (actual_col <= current_line_.size()) {
                current_line_.erase(actual_col - 1, 1);
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
            wksp_->delete_contents(curLine, curLine);
            cursor_line_ = cursor_line_ > 0 ? cursor_line_ - 1 : 0;
            cursor_col_  = prev.size();
        }
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch == KEY_DC) {
        size_t actual_col = get_actual_col();
        if (actual_col < current_line_.size()) {
            current_line_.erase(actual_col, 1);
            current_line_modified_ = true;
        } else if (curLine + 1 < wksp_->total_line_count()) {
            // Join with next line
            get_line(curLine + 1);
            current_line_ += current_line_;
            current_line_no_       = curLine;
            current_line_modified_ = true;
            put_line();
            wksp_->delete_contents(curLine + 1, curLine + 1);
        }
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
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
                // Insert the segments into workspace
                wksp_->insert_contents(temp_segments, curLine + 1);
            } else {
                // Fallback: insert blank line
                auto blank = wksp_->create_blank_lines(1);
                wksp_->insert_contents(blank, curLine + 1);
            }
        } else {
            auto blank = wksp_->create_blank_lines(1);
            wksp_->insert_contents(blank, curLine + 1);
        }
        if (cursor_line_ + 1 < nlines_ - 1) {
            cursor_line_++;
        } else {
            // At bottom of viewport: scroll down
            wksp_->view.topline = wksp_->view.topline + 1;
            // keep cursor on last content row
            cursor_line_ = nlines_ - 2;
        }
        cursor_col_ = 0;
        ensure_cursor_visible();
        return;
    }

    if (ch == '\t') {
        size_t actual_col = get_actual_col();
        current_line_.insert(actual_col, 4, ' ');
        cursor_col_ += 4;
        current_line_modified_ = true;
        put_line();
        ensure_cursor_visible();
        return;
    }

    if (ch >= 32 && ch < 127) {
        size_t actual_col = get_actual_col();
        if (quote_next_) {
            // Quote mode: insert character literally (including control chars shown as ^X)
            char quoted = (char)ch;
            if (ch < 32) {
                quoted = (char)(ch + 64); // Convert to ^A, ^B, etc.
            }
            current_line_.insert(actual_col, 1, quoted);
            cursor_col_++;
            quote_next_ = false;
        } else if (insert_mode_) {
            current_line_.insert(actual_col, 1, (char)ch);
            cursor_col_++;
        } else {
            // Overwrite mode
            if (actual_col < current_line_.size()) {
                current_line_[actual_col] = (char)ch;
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
