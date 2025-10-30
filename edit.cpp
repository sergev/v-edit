#include "editor.h"

//
// Navigate to specified line number.
//
void Editor::goto_line(int lineNumber)
{
    auto total = wksp_->total_line_count();

    if (lineNumber < 0)
        lineNumber = 0;
    if (lineNumber >= total)
        lineNumber = total - 1;
    if (lineNumber < 0)
        lineNumber = 0;

    wksp_->view.topline = lineNumber;
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
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

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
}

//
// Backend editing method: Handle delete operation.
//
void Editor::edit_delete()
{
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

    size_t actual_col = get_actual_col();
    if (actual_col < current_line_.size()) {
        current_line_.erase(actual_col, 1);
        current_line_modified_ = true;
    } else if (curLine + 1 < wksp_->total_line_count()) {
        // Join with next line
        // Preserve current line before loading next
        std::string curr = current_line_;
        get_line(curLine + 1);
        std::string next = current_line_;
        curr += next;
        current_line_          = curr;
        current_line_no_       = curLine;
        current_line_modified_ = true;
        put_line();
        wksp_->delete_contents(curLine + 1, curLine + 1);
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
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

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
            wksp_->insert_contents(temp_segments, curLine + 1);
        } else {
            auto blank = wksp_->create_blank_lines(1);
            wksp_->insert_contents(blank, curLine + 1);
        }
    } else {
        auto blank = wksp_->create_blank_lines(1);
        wksp_->insert_contents(blank, curLine + 1);
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
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

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
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0)
        curLine = 0;
    get_line(curLine);

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
    int curLine = wksp_->view.topline + cursor_line_;
    if (curLine < 0 || curLine >= wksp_->total_line_count()) {
        return 0;
    }
    return (int)wksp_->read_line(curLine).size();
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
    int startLine = wksp_->view.topline + cursor_line_;
    int startCol  = wksp_->view.basecol + cursor_col_;
    auto total    = wksp_->total_line_count();

    // Search from current position forward
    for (int i = startLine; i < total; ++i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = (i == startLine) ? (size_t)startCol : 0;
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
    for (int i = 0; i <= startLine; ++i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = 0;
        if (i == startLine) {
            pos = line.find(needle, (size_t)startCol);
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
    int startLine = wksp_->view.topline + cursor_line_;
    int startCol  = wksp_->view.basecol + cursor_col_;
    auto total    = wksp_->total_line_count();

    // Search from current position backward
    for (int i = startLine; i >= 0; --i) {
        std::string line = wksp_->read_line(i);
        size_t pos       = std::string::npos;
        if (i == startLine) {
            pos = line.rfind(needle, (size_t)startCol);
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
    for (int i = total - 1; i > startLine; --i) {
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
