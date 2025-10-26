#include <ncurses.h>

#include "editor.h"

//
// Start status line color highlighting.
//
void Editor::start_status_color()
{
    // Use colors if available, otherwise use reverse video
    if (has_colors()) {
        attron(COLOR_PAIR(1));
    } else {
        attron(A_REVERSE);
    }
}

//
// End status line color highlighting.
//
void Editor::end_status_color()
{
    // Turn off status line coloring
    if (has_colors()) {
        attroff(COLOR_PAIR(1));
    } else {
        attroff(A_REVERSE);
    }
}

//
// Display status message at bottom of screen.
//
void Editor::draw_status(const std::string &msg)
{
    start_status_color();
    mvhline(nlines - 1, 0, ' ', ncols);
    mvaddnstr(nlines - 1, 0, msg.c_str(), ncols - 1);
    end_status_color();
}

//
// Redraw entire screen and status bar.
//
void Editor::draw()
{
    wksp_redraw();
    move(cursor_line, cursor_col);
    // Build status line dynamically
    if (cmd_mode) {
        if (area_selection_mode) {
            draw_status(status);
        } else {
            std::string s = std::string("Cmd: ") + cmd;
            draw_status(s);
        }
    } else {
        std::string modeStr = insert_mode ? "INSERT" : "OVERWRITE";
        std::string s = std::string("Line=") + std::to_string(wksp.topline() + cursor_line + 1) +
                        "    Col=" + std::to_string(wksp.basecol() + cursor_col + 1) + "    " +
                        modeStr + "    \"" + filename + "\"";
        draw_status(s);
    }
    refresh();
}

//
// Refresh visible lines in the workspace.
//
void Editor::wksp_redraw()
{
    for (int r = 0; r < nlines - 1; ++r) {
        mvhline(r, 0, ' ', ncols);
        std::string lineText;
        if (wksp.chain() != nullptr && r + wksp.topline() < wksp.nlines()) {
            lineText = read_line_from_wksp(r + wksp.topline());
            // horizontal offset and continuation markers
            bool truncated = false;
            if (wksp.basecol() > 0 && (int)lineText.size() > wksp.basecol()) {
                lineText.erase(0, (size_t)wksp.basecol());
            } else if (wksp.basecol() > 0 && (int)lineText.size() <= wksp.basecol()) {
                lineText.clear();
            }
            if ((int)lineText.size() > ncols - 1) {
                truncated = true;
                lineText.resize((size_t)(ncols - 1));
            }
            mvaddnstr(r, 0, lineText.c_str(), ncols - 1);
            if (truncated) {
                mvaddch(r, ncols - 2, '~');
            }
            if (wksp.basecol() > 0 && !lineText.empty()) {
                mvaddch(r, 0, '<');
            }
        } else {
            mvaddch(r, 0, '~');
        }
    }
}

//
// Ensure cursor position is within visible area.
//
void Editor::ensure_cursor_visible()
{
    // First clamp cursor_line to visible range
    if (cursor_line < 0)
        cursor_line = 0;
    if (cursor_line > nlines - 2)
        cursor_line = nlines - 2;

    // Now adjust wksp.topline so that the absolute line is visible
    int absLine      = wksp.topline() + cursor_line;
    int visible_rows = nlines - 1;

    if (absLine < wksp.topline()) {
        // Scroll up to show cursor
        wksp.set_topline(absLine);
    } else if (absLine > wksp.topline() + (visible_rows - 1)) {
        // Scroll down to show cursor
        wksp.set_topline(absLine - (visible_rows - 1));
    }
}
