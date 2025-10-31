#include "editor.h"

//
// Load line from workspace into current line buffer.
//
void Editor::get_line(int lno)
{
    if (current_line_no_ == lno) {
        // We already have this line.
        return;
    }

    // Save any unsaved modifications
    put_line();

    current_line_          = wksp_->read_line(lno);
    current_line_no_       = lno;
    current_line_modified_ = false;
}

//
// Write current line buffer back to workspace if modified.
//
void Editor::put_line()
{
    if (current_line_modified_ && current_line_no_ >= 0) {
        wksp_->put_line(current_line_no_, current_line_);
    }
    current_line_modified_ = false;
}
