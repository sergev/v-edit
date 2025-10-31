#include <fcntl.h>
#include <ncurses.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>

#include "editor.h"
#include "segment.h"

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
    restart_mode_ = restart;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);

    // Initialize colors if available
    if (has_colors()) {
        ::start_color();
        init_pair(int(Color::STATUS), COLOR_BLACK, COLOR_CYAN);       // Status line color pair
        init_pair(int(Color::POSITION), COLOR_YELLOW + 8, COLOR_RED); // Cursor position color pair
        init_pair(int(Color::TRUNCATION), COLOR_YELLOW + 8, COLOR_BLUE); // Truncation color pair
        init_pair(int(Color::EMPTY), COLOR_CYAN, COLOR_BLACK);           // Empty line color pair
    }

    ncols_       = COLS;
    nlines_      = LINES;
    cursor_col_  = 0;
    cursor_line_ = 0;

    // Initialize core data model structures for future segment-based operations
    model_init();

    const std::string user = getUserName();
    const std::string suf  = ttySuffix();
    tmpname_               = std::string("/tmp/ret") + suf + user;
    jname_                 = std::string("/tmp/rej") + suf + user;

    if (restart == 2) {
        // Replay mode: open existing journal
        inputfile_  = ::open(jname_.c_str(), O_RDONLY);
        journal_fd_ = -1;
    } else {
        // Normal or restore: create fresh journal
        unlink(jname_.c_str());
        journal_fd_ = ::creat(jname_.c_str(), 0664);
        if (journal_fd_ >= 0) {
            ::close(journal_fd_);
            journal_fd_ = ::open(jname_.c_str(), O_RDWR);
        }
        inputfile_ = 0; // use stdin
    }
}

//
// Initialize core data structures for segment-based operations.
//
void Editor::model_init()
{
    // Initialize workspaces (passing tempfile reference)
    wksp_     = std::make_unique<Workspace>(tempfile_);
    alt_wksp_ = std::make_unique<Workspace>(tempfile_);

    // Open shared temp file
    tempfile_.open_temp_file();
}

//
// Main event loop and program flow coordinator.
//
int Editor::run(int restart, int argc, char **argv)
{
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

        int ch = journal_read_key();
        if (ch == ERR) {
            // no input, still render
            draw();
        } else {
            if (inputfile_ == 0 && journal_fd_ >= 0) {
                // Record keystroke to journal in normal mode
                journal_write_key(ch);
            }
            // Route key to appropriate handler based on mode.
            if (cmd_mode_) {
                handle_key_cmd(ch);
            } else {
                handle_key_edit(ch);
            }
            draw();
        }
        if (quit_flag_)
            break;
    }

    // Pause for a little bit to make the last status visible.
    refresh();
    usleep(500000);

    // Persist minimal session state before exiting
    save_state();
    endwin();
    if (journal_fd_ >= 0)
        close(journal_fd_);

    // Also emit an exit marker to stdout so tmux capture sees it reliably
    std::cout << "Exiting" << std::endl;
    return 0;
}
