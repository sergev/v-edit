#include <ncurses.h>
#include <unistd.h>

#include <fstream>

#include "editor.h"

//
// Persist current editor state to disk.
//
void Editor::save_state()
{
    std::ofstream out(tmpname_.c_str());
    if (out) {
        out << filename_ << '\n';
        out << wksp_->view.topline << '\n';
        out << wksp_->view.basecol << '\n';
        out << cursor_line_ << '\n';
        out << cursor_col_ << '\n';
        out << insert_mode_ << '\n';
        out << cmd_mode_ << '\n';
        out << cmd_ << '\n';
        out << last_search_ << '\n';
        out << last_search_forward_ << '\n';
        out << wksp_->file_state.backup_done << '\n';
        // Save macro positions
        out << macros_.size() << '\n';
        for (const auto &pair : macros_) {
            out << pair.first << '\n';
            pair.second.serialize(out);
        }
        // Save clipboard_
        clipboard_.serialize(out);
    }
}

//
// Restore previous editor state if requested.
//
void Editor::load_state_if_requested(int restart, int argc, char **argv)
{
    if (restart == 1) {
        // Try to restore from tmpname_
        std::ifstream in(tmpname_.c_str());
        if (in) {
            std::string nm;
            std::getline(in, nm);
            if (!nm.empty()) {
                filename_ = nm;
                wksp_->file_state.backup_done = false; // reset backup flag for restored file
            }
            int topline, basecol;
            in >> topline >> basecol >> cursor_line_ >> cursor_col_;
            wksp_->view.topline = topline;
            wksp_->view.basecol = basecol;
            in >> insert_mode_ >> cmd_mode_;
            std::getline(in, cmd_); // consume newline
            std::getline(in, cmd_);
            std::getline(in, last_search_);
            bool backup_done;
            in >> last_search_forward_ >> backup_done;
            wksp_->file_state.backup_done = backup_done;

            // Load macros_
            size_t macro_count;
            in >> macro_count;
            macros_.clear();
            for (size_t i = 0; i < macro_count; ++i) {
                char name;
                in >> name;
                macros_[name].deserialize(in);
            }

            // Load clipboard_
            clipboard_.deserialize(in);
        }
    }
}

//
// Read next key from input or journal.
//
int Editor::journal_read_key()
{
    if (inputfile_ > 0) {
        // Replay mode: read from journal
        char buf[1];
        ssize_t n = read(inputfile_, buf, 1);
        if (n <= 0) {
            return ERR; // EOF or error
        }
        return (unsigned char)buf[0];
    } else {
        // Normal mode: read from stdin
        return getch();
    }
}

//
// Record key press to journal file.
//
void Editor::journal_write_key(int ch)
{
    if (journal_fd_ >= 0) {
        char buf[1] = { (char)ch };
        write(journal_fd_, buf, 1);
        fsync(journal_fd_);
    }
}
