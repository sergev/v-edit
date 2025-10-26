#include <ncurses.h>
#include <unistd.h>

#include <fstream>

#include "editor.h"

void Editor::save_state()
{
    std::ofstream out(tmpname.c_str());
    if (out) {
        out << filename << '\n';
        out << wksp.topline << '\n';
        out << wksp.offset << '\n';
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
            out << (int)pair.second.type << '\n';
            if (pair.second.type == Macro::POSITION) {
                out << pair.second.position.first << '\n';
                out << pair.second.position.second << '\n';
            } else if (pair.second.type == Macro::BUFFER) {
                out << pair.second.start_line << '\n';
                out << pair.second.end_line << '\n';
                out << pair.second.start_col << '\n';
                out << pair.second.end_col << '\n';
                out << pair.second.is_rectangular << '\n';
                out << pair.second.buffer_lines.size() << '\n';
                for (const std::string &line : pair.second.buffer_lines) {
                    out << line << '\n';
                }
            }
        }
        // Save clipboard
        out << clipboard.is_rectangular << '\n';
        out << clipboard.start_line << '\n';
        out << clipboard.end_line << '\n';
        out << clipboard.start_col << '\n';
        out << clipboard.end_col << '\n';
        out << clipboard.lines.size() << '\n';
        for (const std::string &line : clipboard.lines) {
            out << line << '\n';
        }
    }
}

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
            in >> wksp.topline >> wksp.offset >> cursor_line >> cursor_col;
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
                int type;
                in >> name >> type;
                Macro &m = macros[name];
                m.type   = (Macro::Type)type;
                if (type == Macro::POSITION) {
                    int line, col;
                    in >> line >> col;
                    m.position = std::make_pair(line, col);
                } else if (type == Macro::BUFFER) {
                    in >> m.start_line >> m.end_line >> m.start_col >> m.end_col;
                    in >> m.is_rectangular;
                    size_t line_count;
                    in >> line_count;
                    m.buffer_lines.clear();
                    for (size_t j = 0; j < line_count; ++j) {
                        std::string line;
                        std::getline(in, line); // consume newline
                        std::getline(in, line);
                        m.buffer_lines.push_back(line);
                    }
                }
            }

            // Load clipboard
            in >> clipboard.is_rectangular >> clipboard.start_line >> clipboard.end_line;
            in >> clipboard.start_col >> clipboard.end_col;
            size_t clip_count;
            in >> clip_count;
            clipboard.lines.clear();
            for (size_t i = 0; i < clip_count; ++i) {
                std::string line;
                std::getline(in, line); // consume newline
                std::getline(in, line);
                clipboard.lines.push_back(line);
            }
        }
    }
}

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

void Editor::journal_write_key(int ch)
{
    if (journal_fd >= 0) {
        char buf[1] = { (char)ch };
        write(journal_fd, buf, 1);
        fsync(journal_fd);
    }
}
