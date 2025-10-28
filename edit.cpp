#include "editor.h"

//
// Navigate to specified line number.
//
void Editor::goto_line(int lineNumber)
{
    if (lineNumber < 0)
        lineNumber = 0;
    int total = wksp_->file_state.nlines;
    if (lineNumber >= total)
        lineNumber = total - 1;
    if (lineNumber < 0)
        lineNumber = 0;

    wksp_->view.topline = lineNumber;
    cursor_line_ = 0;
    cursor_col_  = 0;
    wksp_->view.basecol = 0;
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
        wksp_->view.basecol = wksp_->basecol( - 1);
    } else if (cursor_line_ > 0) {
        cursor_line_--;
        int len     = current_line_length();
        cursor_col_ = len;
        if (cursor_col_ >= ncols_ - 1) {
            wksp_->view.basecol = len - (ncols_ - 2);
            cursor_col_ = ncols_ - 2;
        }
    }
}

//
// Move cursor one position right.
//
void Editor::move_right()
{
    int len = current_line_length();
    if (cursor_col_ < len && cursor_col_ < ncols_ - 1) {
        cursor_col_++;
    } else if (cursor_col_ >= ncols_ - 1) {
        wksp_->view.basecol = wksp_->basecol( + 1);
    } else if (cursor_line_ < nlines_ - 2) {
        cursor_line_++;
        cursor_col_ = 0;
        wksp_->view.basecol = 0;
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
        wksp_->view.topline = wksp_->topline( - 1);
    }
    ensure_cursor_visible();
}

//
// Move cursor one line down.
//
void Editor::move_down()
{
    int total = wksp_->file_state.nlines;
    if (cursor_line_ < nlines_ - 2) {
        int absLine = wksp_->view.topline + cursor_line_ + 1;
        if (absLine < total) {
            cursor_line_++;
        }
    } else {
        int absLine = wksp_->view.topline + cursor_line_ + 1;
        if (absLine < total) {
            wksp_->view.topline = wksp_->topline( + 1);
        }
    }
    ensure_cursor_visible();
}

//
// Return length of current line in characters.
//
int Editor::current_line_length() const
{
    int curLine = wksp_->view.topline + cursor_line_;

    // Load the line if not already loaded or if it's a different line
    if (current_line_no_ != curLine) {
        const_cast<Editor *>(this)->get_line(curLine);
    }

    return (int)current_line_.size();
}

//
// Search forward for text pattern.
//
bool Editor::search_forward(const std::string &needle)
{
    int startLine = wksp_->view.topline + cursor_line_;
    int startCol  = wksp_->view.basecol + cursor_col_;
    int total     = wksp_->file_state.nlines;

    // Search from current position forward
    for (int i = startLine; i < total; ++i) {
        std::string line = read_line_from_wksp(i);
        size_t pos       = (i == startLine) ? (size_t)startCol : 0;
        pos              = line.find(needle, pos);
        if (pos != std::string::npos) {
            // Found it - position cursor
            wksp_->view.topline = i;
            cursor_line_ = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (intpos - (ncols_ - 10));
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
        std::string line = read_line_from_wksp(i);
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
            cursor_line_ = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (intpos - (ncols_ - 10));
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
    int total     = wksp_->file_state.nlines;

    // Search from current position backward
    for (int i = startLine; i >= 0; --i) {
        std::string line = read_line_from_wksp(i);
        size_t pos       = std::string::npos;
        if (i == startLine) {
            pos = line.rfind(needle, (size_t)startCol);
        } else {
            pos = line.rfind(needle);
        }
        if (pos != std::string::npos) {
            wksp_->view.topline = i;
            cursor_line_ = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (intpos - (ncols_ - 10));
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
        std::string line = read_line_from_wksp(i);
        size_t pos       = line.rfind(needle);
        if (pos != std::string::npos) {
            wksp_->view.topline = i;
            cursor_line_ = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols_ - 10)) {
                wksp_->view.basecol = (intpos - (ncols_ - 10));
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

    ensure_line_saved();

    // Insert blank lines using workspace segments
    auto blank = wksp_->create_blank_lines(number);
    wksp_->insert_segments(blank, from);
    wksp_->file_state.nlines = wksp_->nlines( + number);

    ensure_cursor_visible();
}

//
// Delete lines starting at specified position.
//
void Editor::deletelines(int from, int number)
{
    if (from < 0 || number < 1)
        return;

    ensure_line_saved();

    // Save to clipboard (delete buffer)
    picklines(from, number);

    // Delete the lines using workspace segments
    wksp_->delete_segments(from, from + number - 1);
    wksp_->file_state.nlines = std::max(0, wksp_->nlines( - number));

    // Ensure at least one line exists
    if (wksp_->file_state.nlines == 0) {
        auto blank = wksp_->create_blank_lines(1);
        wksp_->insert_segments(blank, 0);
        wksp_->file_state.nlines = 1;
    }

    ensure_cursor_visible();
}

//
// Split line into two at cursor position.
//
void Editor::splitline(int line, int col)
{
    if (line < 0 || col < 0)
        return;

    ensure_line_saved();

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
        wksp_->get_segments().splice(wksp_->get_segments().end(), temp_segments);
        wksp_->file_state.nlines = wksp_->nlines( + 1);
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

    if (line + 1 >= wksp_->file_state.nlines)
        return; // No next line to combine

    ensure_line_saved();

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
    wksp_->delete_segments(line + 1, line + 1);
    wksp_->file_state.nlines = std::max(1, wksp_->nlines( - 1));

    ensure_cursor_visible();
}
