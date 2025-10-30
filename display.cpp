#include <ncurses.h>

#include "editor.h"

//
// Start status line color highlighting.
//
void Editor::start_color(Color pair)
{
    // Use colors if available, otherwise use reverse video
    if (has_colors()) {
        attron(COLOR_PAIR(pair));
    } else {
        attron(A_REVERSE);
    }
}

//
// End status line color highlighting.
//
void Editor::end_color(Color pair)
{
    // Turn off status line coloring
    if (has_colors()) {
        attroff(COLOR_PAIR(pair));
    } else {
        attroff(A_REVERSE);
    }
}

//
// Display status message at bottom of screen.
//
void Editor::draw_status(const std::string &msg)
{
    start_color(Color::STATUS);
    mvhline(nlines_ - 1, 0, ' ', ncols_);
    mvaddnstr(nlines_ - 1, 0, msg.c_str(), ncols_ - 1);
    end_color(Color::STATUS);
}

//
// Display position of cursor.
//
void Editor::draw_tag()
{
    if (area_selection_mode_) {
        int r, c;
        params_.get_opposite_corner(cursor_line_ + wksp_->view.topline,
                                    cursor_col_ + wksp_->view.basecol, r, c);
        r -= wksp_->view.topline;
        c -= wksp_->view.basecol;
        if (r >= 0 && c >= 0 && r < nlines_ - 1 && c < ncols_) {
            // The opposite corner is visible.
            start_color(Color::POSITION);
            mvaddch(r, c, '@');
            end_color(Color::POSITION);
        }
    } else {
        start_color(Color::POSITION);
        mvaddch(cursor_line_, cursor_col_, '@');
        end_color(Color::POSITION);
    }
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
            bool clipped = false;
            bool truncated = false;
            if (wksp_->view.basecol > 0 && (int)lineText.size() > wksp_->view.basecol) {
                lineText.erase(0, (size_t)wksp_->view.basecol);
                clipped = true;
            } else if (wksp_->view.basecol > 0 && (int)lineText.size() <= wksp_->view.basecol) {
                // Beyond line content - show blank spaces (virtual column position)
                lineText.clear();
                clipped = true;
            }
            if ((int)lineText.size() > ncols_ - 1) {
                truncated = true;
                lineText.resize((size_t)(ncols_ - 1));
            }
            mvaddnstr(r, 0, lineText.c_str(), ncols_ - 1);
            if (truncated) {
                start_color(Color::TRUNCATION);
                mvaddch(r, ncols_ - 2, '~');
                end_color(Color::TRUNCATION);
            }
            if (clipped) {
                start_color(Color::TRUNCATION);
                mvaddch(r, 0, '<');
                end_color(Color::TRUNCATION);
            }
        } else {
            // Beyond end of file - show virtual line marker
            start_color(Color::EMPTY);
            mvaddch(r, 0, '~');
            end_color(Color::EMPTY);
        }
    }
}

//
// Ensure cursor position is within visible area.
//
void Editor::ensure_cursor_visible()
{
    // First clamp cursor_line_ to visible range (screen position)
    if (cursor_line_ < 0)
        cursor_line_ = 0;
    if (cursor_line_ > nlines_ - 2)
        cursor_line_ = nlines_ - 2;

    // Now adjust wksp_->topline so that the absolute line is visible
    // Allow absLine to exceed total_line_count() for virtual positions
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
