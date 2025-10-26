#include "editor.h"

//
// Navigate to specified line number.
//
void Editor::goto_line(int lineNumber)
{
    if (lineNumber < 0)
        lineNumber = 0;
    int total = wksp.get_line_count((int)lines.size());
    if (lineNumber >= total)
        lineNumber = total - 1;
    if (lineNumber < 0)
        lineNumber = 0;

    wksp.set_topline(lineNumber);
    cursor_line = 0;
    cursor_col  = 0;
    wksp.set_basecol(0);
    ensure_cursor_visible();
}

//
// Move cursor one position left.
//
void Editor::move_left()
{
    if (cursor_col > 0) {
        cursor_col--;
    } else if (wksp.basecol() > 0) {
        wksp.set_basecol(wksp.basecol() - 1);
    } else if (cursor_line > 0) {
        cursor_line--;
        int len    = current_line_length();
        cursor_col = len;
        if (cursor_col >= ncols - 1) {
            wksp.set_basecol(len - (ncols - 2));
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
        wksp.set_basecol(wksp.basecol() + 1);
    } else if (cursor_line < nlines - 2) {
        cursor_line++;
        cursor_col = 0;
        wksp.set_basecol(0);
    }
}

//
// Move cursor one line up.
//
void Editor::move_up()
{
    if (cursor_line > 0) {
        cursor_line--;
    } else if (wksp.topline() > 0) {
        wksp.set_topline(wksp.topline() - 1);
    }
    ensure_cursor_visible();
}

//
// Move cursor one line down.
//
void Editor::move_down()
{
    int total = wksp.get_line_count((int)lines.size());
    if (cursor_line < nlines - 2) {
        int absLine = wksp.topline() + cursor_line + 1;
        if (absLine < total) {
            cursor_line++;
        }
    } else {
        int absLine = wksp.topline() + cursor_line + 1;
        if (absLine < total) {
            wksp.set_topline(wksp.topline() + 1);
        }
    }
    ensure_cursor_visible();
}

//
// Return length of current line in characters.
//
int Editor::current_line_length() const
{
    int curLine = wksp.topline() + cursor_line;
    if (curLine >= 0 && curLine < (int)lines.size()) {
        return (int)lines[curLine].size();
    }
    return 0;
}

//
// Search forward for text pattern.
//
bool Editor::search_forward(const std::string &needle)
{
    int startLine = wksp.topline() + cursor_line;
    int startCol  = wksp.basecol() + cursor_col;

    // Search from current position forward
    for (int i = startLine; i < (int)lines.size(); ++i) {
        std::string line = get_line_from_model(i);
        size_t pos       = (i == startLine) ? (size_t)startCol : 0;
        pos              = line.find(needle, pos);
        if (pos != std::string::npos) {
            // Found it - position cursor
            wksp.set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.set_basecol((int)pos - (ncols - 10));
            } else {
                wksp.set_basecol(0);
            }
            cursor_col = (int)pos - wksp.basecol();
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    // Wrap around to beginning
    for (int i = 0; i <= startLine; ++i) {
        std::string line = get_line_from_model(i);
        size_t pos       = 0;
        if (i == startLine) {
            pos = line.find(needle, (size_t)startCol);
            if (pos == std::string::npos)
                continue;
        } else {
            pos = line.find(needle);
        }
        if (pos != std::string::npos) {
            wksp.set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.set_basecol((int)pos - (ncols - 10));
            } else {
                wksp.set_basecol(0);
            }
            cursor_col = (int)pos - wksp.basecol();
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
    int startLine = wksp.topline() + cursor_line;
    int startCol  = wksp.basecol() + cursor_col;

    // Search from current position backward
    for (int i = startLine; i >= 0; --i) {
        std::string line = get_line_from_model(i);
        size_t pos       = std::string::npos;
        if (i == startLine) {
            pos = line.rfind(needle, (size_t)startCol);
        } else {
            pos = line.rfind(needle);
        }
        if (pos != std::string::npos) {
            wksp.set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.set_basecol((int)pos - (ncols - 10));
            } else {
                wksp.set_basecol(0);
            }
            cursor_col = (int)pos - wksp.basecol();
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    // Wrap around to end
    for (int i = (int)lines.size() - 1; i > startLine; --i) {
        std::string line = get_line_from_model(i);
        size_t pos       = line.rfind(needle);
        if (pos != std::string::npos) {
            wksp.set_topline(i);
            cursor_line = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.set_basecol((int)pos - (ncols - 10));
            } else {
                wksp.set_basecol(0);
            }
            cursor_col = (int)pos - wksp.basecol();
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

    ensure_segments_up_to_date();

    // Insert blank lines
    for (int i = 0; i < number; ++i) {
        if (from >= (int)lines.size()) {
            lines.push_back("");
        } else {
            lines.insert(lines.begin() + from + i, "");
        }
    }

    wksp.set_nlines(wksp.nlines() + number);

    build_segment_chain_from_lines();
    segments_dirty = true;
    ensure_cursor_visible();
}

//
// Delete lines starting at specified position.
//
void Editor::deletelines(int from, int number)
{
    if (from < 0 || number < 1)
        return;

    ensure_segments_up_to_date();

    // Save to clipboard (delete buffer)
    clipboard.clear();
    clipboard.is_rectangular = false;
    clipboard.start_line     = from;
    clipboard.end_line       = from + number - 1;

    for (int i = 0; i < number && (from + i) < (int)lines.size(); ++i) {
        clipboard.lines.push_back(lines[from + i]);
    }

    // Delete the lines
    int end_line = std::min(from + number, (int)lines.size());
    if (from < (int)lines.size()) {
        lines.erase(lines.begin() + from, lines.begin() + end_line);
    }

    // Ensure at least one line exists
    if (lines.empty()) {
        lines.push_back("");
    }

    wksp.set_nlines(std::max(0, wksp.nlines() - number));

    build_segment_chain_from_lines();
    segments_dirty = true;
    ensure_cursor_visible();
}

//
// Split line into two at cursor position.
//
void Editor::splitline(int line, int col)
{
    if (line < 0 || col < 0)
        return;

    ensure_segments_up_to_date();

    // Get the line
    std::string ln;
    if (line < (int)lines.size()) {
        ln = lines[line];
    } else {
        ln = "";
    }

    // Split at column
    if (col >= (int)ln.size()) {
        // Just insert a blank line after
        insertlines(line + 1, 1);
        return;
    }

    std::string tail = ln.substr(col);
    ln.erase(col);

    // Update current line
    if (line < (int)lines.size()) {
        lines[line] = ln;
    }

    // Insert new line with tail
    if (line + 1 < (int)lines.size()) {
        lines.insert(lines.begin() + line + 1, tail);
    } else {
        lines.push_back(tail);
    }

    wksp.set_nlines(wksp.nlines() + 1);

    build_segment_chain_from_lines();
    segments_dirty = true;
    ensure_cursor_visible();
}

//
// Combine current line with next line at cursor position.
//
void Editor::combineline(int line, int col)
{
    if (line < 0 || col < 0)
        return;

    if (line + 1 >= (int)lines.size())
        return; // No next line to combine

    ensure_segments_up_to_date();

    // Get both lines
    std::string current = (line < (int)lines.size()) ? lines[line] : "";
    std::string next    = (line + 1 < (int)lines.size()) ? lines[line + 1] : "";

    // Combine at column
    // Pad current line to col if needed
    if (col > (int)current.size()) {
        current.resize(col, ' ');
    }

    current += next;

    // Update line
    lines[line] = current;

    // Delete the next line
    lines.erase(lines.begin() + line + 1);

    wksp.set_nlines(std::max(1, wksp.nlines() - 1));

    build_segment_chain_from_lines();
    segments_dirty = true;
    ensure_cursor_visible();
}
