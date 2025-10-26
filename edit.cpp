#include "editor.h"

void Editor::goto_line(int lineNumber)
{
    if (lineNumber < 0)
        lineNumber = 0;
    int total =
        !files.empty() && files[wksp.wfile].chain ? files[wksp.wfile].nlines : (int)lines.size();
    if (lineNumber >= total)
        lineNumber = total - 1;
    if (lineNumber < 0)
        lineNumber = 0;

    wksp.topline = lineNumber;
    cursor_line  = 0;
    cursor_col   = 0;
    wksp.offset  = 0;
    ensure_cursor_visible();
}

void Editor::move_left()
{
    if (cursor_col > 0) {
        cursor_col--;
    } else if (wksp.offset > 0) {
        wksp.offset--;
    } else if (cursor_line > 0) {
        cursor_line--;
        int len    = current_line_length();
        cursor_col = len;
        if (cursor_col >= ncols - 1) {
            wksp.offset = len - (ncols - 2);
            cursor_col  = ncols - 2;
        }
    }
}

void Editor::move_right()
{
    int len = current_line_length();
    if (cursor_col < len && cursor_col < ncols - 1) {
        cursor_col++;
    } else if (cursor_col >= ncols - 1) {
        wksp.offset++;
    } else if (cursor_line < nlines - 2) {
        cursor_line++;
        cursor_col  = 0;
        wksp.offset = 0;
    }
}

void Editor::move_up()
{
    if (cursor_line > 0) {
        cursor_line--;
    } else if (wksp.topline > 0) {
        wksp.topline--;
    }
    ensure_cursor_visible();
}

void Editor::move_down()
{
    int total =
        !files.empty() && files[wksp.wfile].chain ? files[wksp.wfile].nlines : (int)lines.size();
    if (cursor_line < nlines - 2) {
        int absLine = wksp.topline + cursor_line + 1;
        if (absLine < total) {
            cursor_line++;
        }
    } else {
        int absLine = wksp.topline + cursor_line + 1;
        if (absLine < total) {
            wksp.topline++;
        }
    }
    ensure_cursor_visible();
}

int Editor::current_line_length() const
{
    int curLine = wksp.topline + cursor_line;
    if (curLine >= 0 && curLine < (int)lines.size()) {
        return (int)lines[curLine].size();
    }
    return 0;
}

bool Editor::search_forward(const std::string &needle)
{
    int startLine = wksp.topline + cursor_line;
    int startCol  = wksp.offset + cursor_col;

    // Search from current position forward
    for (int i = startLine; i < (int)lines.size(); ++i) {
        std::string line = get_line_from_model(i);
        size_t pos       = (i == startLine) ? (size_t)startCol : 0;
        pos              = line.find(needle, pos);
        if (pos != std::string::npos) {
            // Found it - position cursor
            wksp.topline = i;
            cursor_line  = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.offset = (int)pos - (ncols - 10);
            } else {
                wksp.offset = 0;
            }
            cursor_col = (int)pos - wksp.offset;
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
            wksp.topline = i;
            cursor_line  = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.offset = (int)pos - (ncols - 10);
            } else {
                wksp.offset = 0;
            }
            cursor_col = (int)pos - wksp.offset;
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    status = std::string("Not found: ") + needle;
    return false;
}

bool Editor::search_backward(const std::string &needle)
{
    int startLine = wksp.topline + cursor_line;
    int startCol  = wksp.offset + cursor_col;

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
            wksp.topline = i;
            cursor_line  = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.offset = (int)pos - (ncols - 10);
            } else {
                wksp.offset = 0;
            }
            cursor_col = (int)pos - wksp.offset;
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
            wksp.topline = i;
            cursor_line  = 0;
            // Only set horizontal offset if the match is far to the right
            if (pos > (size_t)(ncols - 10)) {
                wksp.offset = (int)pos - (ncols - 10);
            } else {
                wksp.offset = 0;
            }
            cursor_col = (int)pos - wksp.offset;
            ensure_cursor_visible();
            status = std::string("Found: ") + needle;
            return true;
        }
    }

    status = std::string("Not found: ") + needle;
    return false;
}

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
