#include <ncurses.h>
#include <unistd.h>

#include <fstream>

#include "editor.h"

//
// Persist current editor state to disk.
//
void Editor::save_state()
{
    std::ofstream out(tmpname.c_str());
    if (out) {
        out << filename << '\n';
        out << wksp.topline() << '\n';
        out << wksp.basecol() << '\n';
        out << cursor_line << '\n';
        out << cursor_col << '\n';
        out << insert_mode << '\n';
        out << cmd_mode << '\n';
        out << cmd << '\n';
        out << last_search << '\n';
        out << last_search_forward << '\n';
        out << backup_done << '\n';
        out << segments_dirty << '\n';
        // Save macro positions
        out << macros.size() << '\n';
        for (const auto &pair : macros) {
            out << pair.first << '\n';
            pair.second.serialize(out);
        }
        // Save clipboard
        clipboard.serialize(out);
    }
}

//
// Restore previous editor state if requested.
//
void Editor::load_state_if_requested(int restart, int argc, char **argv)
{
    if (restart == 1) {
        // Try to restore from tmpname
        std::ifstream in(tmpname.c_str());
        if (in) {
            std::string nm;
            std::getline(in, nm);
            if (!nm.empty()) {
                filename    = nm;
                backup_done = false; // reset backup flag for restored file
            }
            int topline, basecol;
            in >> topline >> basecol >> cursor_line >> cursor_col;
            wksp.set_topline(topline);
            wksp.set_basecol(basecol);
            in >> insert_mode >> cmd_mode;
            std::getline(in, cmd); // consume newline
            std::getline(in, cmd);
            std::getline(in, last_search);
            in >> last_search_forward >> backup_done >> segments_dirty;

            // Load macros
            size_t macro_count;
            in >> macro_count;
            macros.clear();
            for (size_t i = 0; i < macro_count; ++i) {
                char name;
                in >> name;
                macros[name].deserialize(in);
            }

            // Load clipboard
            clipboard.deserialize(in);
        }
    }
}

//
// Read next key from input or journal.
//
int Editor::journal_read_key()
{
    if (inputfile > 0) {
        // Replay mode: read from journal
        char buf[1];
        ssize_t n = read(inputfile, buf, 1);
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
    if (journal_fd >= 0) {
        char buf[1] = { (char)ch };
        write(journal_fd, buf, 1);
        fsync(journal_fd);
    }
}
