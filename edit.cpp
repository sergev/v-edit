#include "editor.h"

//
// Navigate to specified line number.
//
void Editor::goto_line(int lineNumber)
{
    if (lineNumber < 0)
        lineNumber = 0;
    int total = wksp->nlines();
    if (lineNumber >= total)
        lineNumber = total - 1;
    if (lineNumber < 0)
        lineNumber = 0;

    wksp->set_topline(lineNumber);
    cursor_line = 0;
    cursor_col  = 0;
    wksp->set_basecol(0);
    ensure_cursor_visible();
}

//
// Move cursor one position left.
//
void Editor::move_left()
{
    if (cursor_col > 0) {
        cursor_col--;
    } else if (wksp->basecol() > 0) {
        wksp->set_basecol(wksp->basecol() - 1);
    } else if (cursor_line > 0) {
        cursor_line--;
        int len    = current_line_length();
        cursor_col = len;
        if (cursor_col >= ncols - 1) {
            wksp->set_basecol(len - (ncols - 2));
            cursor_col = ncols - 2;
        }
    }
}

//
// Move cursor one position right.
//
void Editor::move_right()
{
    int len = current_line_length();
    if (cursor_col < len && cursor_col < ncols - 1) {
        cursor_col++;
    } else if (cursor_col >= ncols - 1) {
        wksp->set_basecol(wksp->basecol() + 1);
    } else if (cursor_line < nlines - 2) {
        cursor_line++;
        cursor_col = 0;
        wksp->set_basecol(0);
    }
}

//
// Move cursor one line up.
//
void Editor::move_up()
{
    if (cursor_line > 0) {
        cursor_line--;
    } else if (wksp->topline() > 0) {
        wksp->set_topline(wksp->topline() - 1);
    }
    ensure_cursor_visible();
}

//
// Move cursor one line down.
//
void Editor::move_down()
{
    int total = wksp->nlines();
    if (cursor_line < nlines - 2) {
        int absLine = wksp->topline() + cursor_line + 1;
        if (absLine < total) {
            cursor_line++;
        }
    } else {
        int absLine = wksp->topline() + cursor_line + 1;
        if (absLine < total) {
            wksp->set_topline(wksp->topline() + 1);
        }
    }
    ensure_cursor_visible();
}

//
// Return length of current line in characters.
//
int Editor::current_line_length() const
{
    int curLine = wksp->topline() + cursor_line;

    // Load the line if not already loaded or if it's a different line
    if (current_line_no != curLine) {
        const_cast<Editor *>(this)->get_line(curLine);
    }

    return (int)current_line.size();
}

//
// Search forward for text pattern.
//
bool Editor::search_forward(const std::string &needle)
{
    int startLine = wksp->topline() + cursor_line;
    int startCol  = wksp->basecol() + cursor_col;
    int total     = wksp->nlines();

    // Search from current position forward
    for (int i = startLine; i < total; ++i) {
        std::string line = read_line_from_wksp(i);
        size_t pos       = (i == startLine) ? (size_t)startCol : 0;
        pos              = line.find(needle, pos);
        if (pos != std::string::npos) {
            // Found it - position cursor
            wksp->set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp->set_basecol((int)pos - (ncols - 10));
            } else {
                wksp->set_basecol(0);
            }
            cursor_col = (int)pos - wksp->basecol();
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
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
            wksp->set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp->set_basecol((int)pos - (ncols - 10));
            } else {
                wksp->set_basecol(0);
            }
            cursor_col = (int)pos - wksp->basecol();
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    status = std::string("Not found: ") + needle;
    return false;
}

//
// Search backward for text pattern.
//
bool Editor::search_backward(const std::string &needle)
{
    int startLine = wksp->topline() + cursor_line;
    int startCol  = wksp->basecol() + cursor_col;
    int total     = wksp->nlines();

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
            wksp->set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp->set_basecol((int)pos - (ncols - 10));
            } else {
                wksp->set_basecol(0);
            }
            cursor_col = (int)pos - wksp->basecol();
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    // Wrap around to end
    for (int i = total - 1; i > startLine; --i) {
        std::string line = read_line_from_wksp(i);
        size_t pos       = line.rfind(needle);
        if (pos != std::string::npos) {
            wksp->set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp->set_basecol((int)pos - (ncols - 10));
            } else {
                wksp->set_basecol(0);
            }
            cursor_col = (int)pos - wksp->basecol();
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    status = std::string("Not found: ") + needle;
    return false;
}

//
// Repeat last search in same direction.
//
bool Editor::search_next()
{
    if (!last_search.empty()) {
        if (last_search_forward) {
            return search_forward(last_search);
        } else {
            return search_backward(last_search);
        }
    }
    return false;
}

//
// Repeat last search in opposite direction.
//
bool Editor::search_prev()
{
    if (!last_search.empty()) {
        if (last_search_forward) {
            return search_backward(last_search);
        } else {
            return search_forward(last_search);
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
    Segment *blank = wksp->create_blank_lines(number);
    wksp->insert_segments(blank, from);
    wksp->set_nlines(wksp->nlines() + number);

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
    wksp->delete_segments(from, from + number - 1);
    wksp->set_nlines(std::max(0, wksp->nlines() - number));

    // Ensure at least one line exists
    if (wksp->nlines() == 0) {
        Segment *blank = wksp->create_blank_lines(1);
        wksp->insert_segments(blank, 0);
        wksp->set_nlines(1);
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

    // Get the line using current_line buffer
    get_line(line);
    std::string ln = current_line;

    // Split at column
    if (col >= (int)ln.size()) {
        // Just insert a blank line after
        insertlines(line + 1, 1);
        return;
    }

    std::string tail = ln.substr(col);
    ln.erase(col);

    // Update current line
    current_line          = ln;
    current_line_no       = line;
    current_line_modified = true;
    put_line();

    // Insert new line with tail content
    Segment *tail_seg = wksp->write_line_to_temp(tail);
    if (tail_seg) {
        wksp->insert_segments(tail_seg, line + 1);
        wksp->set_nlines(wksp->nlines() + 1);
    } else {
        // Fallback: insert blank line
        insertlines(line + 1, 1);
        wksp->set_nlines(wksp->nlines() + 1);
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

    if (line + 1 >= wksp->nlines())
        return; // No next line to combine

    ensure_line_saved();

    // Get both lines
    get_line(line);
    std::string current = current_line;
    get_line(line + 1);
    std::string next = current_line;

    // Combine at column
    // Pad current line to col if needed
    if (col > (int)current.size()) {
        current.resize(col, ' ');
    }

    current += next;

    // Update line
    current_line          = current;
    current_line_no       = line;
    current_line_modified = true;
    put_line();

    // Delete the next line
    wksp->delete_segments(line + 1, line + 1);
    wksp->set_nlines(std::max(1, wksp->nlines() - 1));

    ensure_cursor_visible();
}
