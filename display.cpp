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
// Start color highlighting for cursor position.
//
void Editor::start_tag_color()
{
    // Use colors if available, otherwise use reverse video
    if (has_colors()) {
        attron(COLOR_PAIR(2));
    } else {
        attron(A_REVERSE);
    }
}

//
// End color highlighting for cursor position.
//
void Editor::end_tag_color()
{
    // Turn off tag line coloring
    if (has_colors()) {
        attroff(COLOR_PAIR(2));
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
    mvhline(nlines_ - 1, 0, ' ', ncols_);
    mvaddnstr(nlines_ - 1, 0, msg.c_str(), ncols_ - 1);
    end_status_color();
}

//
// Display position of cursor.
//
void Editor::draw_tag()
{
    start_tag_color();
    if (area_selection_mode_) {
        int c0, r0;
        params_.get_area_start(c0, r0);
        move(r0, c0);
    } else {
        move(cursor_line_, cursor_col_);
    }
    addch('@');
    end_tag_color();
}

//
// Redraw entire screen and status bar.
//
void Editor::draw()
{
    wksp_redraw();

    // Build status line dynamically
    if (cmd_mode_) {
        if (area_selection_mode_) {
            draw_status(status_);
        } else {
            std::string s = std::string("Cmd: ") + cmd_;
            draw_status(s);
        }
        draw_tag();
    } else if (!status_.empty()) {
        // Draw status once.
        draw_status(status_);
        status_ = "";
    } else {
        std::string modeStr = insert_mode_ ? "INSERT" : "OVERWRITE";
        std::string s       = std::string("Line=") +
                        std::to_string(wksp_->view.topline + cursor_line_ + 1) +
                        "    Col=" + std::to_string(wksp_->view.basecol + cursor_col_ + 1) +
                        "    " + modeStr + "    \"" + filename_ + "\"";
        draw_status(s);
    }

    // Position cursor at current line and column before reading input
    if (cmd_mode_ && !area_selection_mode_) {
        move(nlines_ - 1, 5 + cmd_.size());
    } else {
        move(cursor_line_, cursor_col_);
    }
    refresh();
}

//
// Refresh visible lines in the workspace.
//
void Editor::wksp_redraw()
{
    auto total = wksp_->total_line_count();
    for (int r = 0; r < nlines_ - 1; ++r) {
        mvhline(r, 0, ' ', ncols_);
        std::string lineText;
        if (r + wksp_->view.topline < total) {
            lineText = wksp_->read_line(r + wksp_->view.topline);
            // horizontal offset and continuation markers
            bool truncated = false;
            if (wksp_->view.basecol > 0 && (int)lineText.size() > wksp_->view.basecol) {
                lineText.erase(0, (size_t)wksp_->view.basecol);
            } else if (wksp_->view.basecol > 0 && (int)lineText.size() <= wksp_->view.basecol) {
                lineText.clear();
            }
            if ((int)lineText.size() > ncols_ - 1) {
                truncated = true;
                lineText.resize((size_t)(ncols_ - 1));
            }
            mvaddnstr(r, 0, lineText.c_str(), ncols_ - 1);
            if (truncated) {
                mvaddch(r, ncols_ - 2, '~');
            }
            if (wksp_->view.basecol > 0 && !lineText.empty()) {
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
    // First clamp cursor_line_ to visible range
    if (cursor_line_ < 0)
        cursor_line_ = 0;
    if (cursor_line_ > nlines_ - 2)
        cursor_line_ = nlines_ - 2;

    // Now adjust wksp_->topline so that the absolute line is visible
    int absLine      = wksp_->view.topline + cursor_line_;
    int visible_rows = nlines_ - 1;

    if (absLine < wksp_->view.topline) {
        // Scroll up to show cursor
        wksp_->view.topline = absLine;
    } else if (absLine > wksp_->view.topline + (visible_rows - 1)) {
        // Scroll down to show cursor
        wksp_->view.topline = absLine - (visible_rows - 1);
    }
}
