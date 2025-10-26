#include <fcntl.h>
#include <ncurses.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>

#include "editor.h"

// Help file system
const std::string Editor::DEFAULT_HELP_FILE = "/usr/share/ve/help";

Editor::Editor()
{
}

//
// Get current user's name.
//
static std::string getUserName()
{
    const char *u = getenv("USER");
    if (u)
        return std::string(u);
    struct passwd *pw = getpwuid(getuid());
    return pw ? std::string(pw->pw_name) : std::string("user");
}

//
// Generate TTY suffix for temporary file names.
//
static std::string ttySuffix()
{
    const char *tty = ttyname(0);
    if (!tty)
        return std::string("notty");
    std::string s(tty);
    if (s.size() >= 2) {
        if (s[s.size() - 2] == '/')
            s[s.size() - 2] = '-';
    }
    return s.substr(s.size() > 2 ? s.size() - 2 : 0);
}

//
// Initialize ncurses and set up terminal for editing.
//
void Editor::startup(int restart)
{
    restart_mode = restart;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);

    // Initialize colors if available
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_BLACK, COLOR_CYAN); // Status line color pair
    }

    ncols       = COLS;
    nlines      = LINES;
    cursor_col  = 0;
    cursor_line = 0;

    // Initialize core data model structures for future segment-based operations
    model_init();

    const std::string user = getUserName();
    const std::string suf  = ttySuffix();
    tmpname                = std::string("/tmp/ret") + suf + user;
    jname                  = std::string("/tmp/rej") + suf + user;
    rfile                  = std::string("/tmp/res") + suf + user;

    if (restart == 2) {
        // Replay mode: open existing journal
        inputfile  = ::open(jname.c_str(), O_RDONLY);
        journal_fd = -1;
    } else {
        // Normal or restore: create fresh journal
        unlink(jname.c_str());
        journal_fd = ::creat(jname.c_str(), 0664);
        if (journal_fd >= 0) {
            ::close(journal_fd);
            journal_fd = ::open(jname.c_str(), O_RDWR);
        }
        inputfile = 0; // use stdin
    }
}

//
// Initialize core data structures for segment-based operations.
//
void Editor::model_init()
{
    // Initialize workspaces
    wksp     = std::make_unique<Workspace>();
    alt_wksp = std::make_unique<Workspace>();

    // Open temp files for workspaces
    wksp->open_temp_file();
    alt_wksp->open_temp_file();
}

//
// Main event loop and program flow coordinator.
//
int Editor::run(int argc, char **argv)
{
    int restart = 0;
    if (argc == 1)
        restart = 1; // restore attempt
    else if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '\0')
        restart = 2; // replay

    startup(restart);

    // Setup signal handlers
    setup_signal_handlers();

    load_state_if_requested(restart, argc, argv);
    open_initial(argc, argv);
    draw();

    // simple loop sufficient for smoke test
    timeout(200);
    for (;;) {
        // Check for interrupts
        check_interrupt();

        // Position cursor at current line and column before reading input
        move(cursor_line, cursor_col);

        int ch = journal_read_key();
        if (ch == ERR) {
            // no input, still render
            draw();
        } else {
            if (inputfile == 0 && journal_fd >= 0) {
                // Record keystroke to journal in normal mode
                journal_write_key(ch);
            }
            handle_key(ch);
            draw();
        }
        if (quit_flag)
            break;
    }

    refresh();
    usleep(100000);
    // Persist minimal session state before exiting
    save_state();
    // Also emit an exit marker to stdout so tmux capture sees it reliably
    endwin();
    fputs("Exiting\n", stdout);
    fflush(stdout);
    usleep(200000);
    if (journal_fd >= 0)
        close(journal_fd);
    return 0;
}
