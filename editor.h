#ifndef EDITOR_H
#define EDITOR_H

#include <map>
#include <memory>
#include <string>

#include "clipboard.h"
#include "macro.h"
#include "parameters.h"
#include "segment.h"
#include "tempfile.h"
#include "workspace.h"

class Editor {
public:
    Editor();
    int run(int restart, int argc, char **argv);

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_H_
private:
#endif
    static Editor *instance_; // For signal handler access

    // Minimal single-window state
    int ncols_{};
    int nlines_{};
    int cursor_col_{};
    int cursor_line_{};
    std::string status_;
    std::string filename_{ "untitled" };
    bool cmd_mode_{ false };
    bool quit_flag_{ false };

    // Current line buffer (prototype's cline pattern)
    std::string current_line_;
    int current_line_no_{ -1 };
    bool current_line_modified_{ false };

    bool filter_mode_{ false };         // track if we're in filter command mode
    bool area_selection_mode_{ false }; // track if we're in area selection mode
    std::string cmd_;

    // Enhanced parameter system
    Parameters params_;
    std::string last_search_; // last search needle
    bool last_search_forward_{ true };
    std::vector<std::string> clipboard_lines_; // simple line clipboard (F5/F6)
    bool quote_next_{ false };                 // ^P - quote next character literally
    bool ctrlx_state_{ false };                // ^X prefix state
    bool insert_mode_{ true };                 // insert vs overwrite mode

    // Signal handling
    bool interrupt_flag_{ false }; // interrupt signal occurred

    // Temporary file management (shared by all workspaces)
    Tempfile tempfile_;

    // Two workspaces: main workspace (wksp) and alternative workspace (alt_wksp)
    std::unique_ptr<Workspace> wksp_;
    std::unique_ptr<Workspace> alt_wksp_;
    std::string alt_filename_;

    // Enhanced clipboard (supports line ranges)
    Clipboard clipboard_;

    // Macros (position markers and text buffers)
    std::map<char, Macro> macros_; // char -> macro data

    // Journaling
    int journal_fd_{ -1 };
    std::string jname_;
    std::string tmpname_;
    int inputfile_{ 0 };    // 0=stdin, >=0=journal file fd for replay
    int restart_mode_{ 0 }; // 0=normal, 1=restore, 2=replay

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_H_
private:
#endif
    // Help file installed in a public place
    static const std::string DEFAULT_HELP_FILE;

    // Startup and display
    //
    // Initialize ncurses terminal.
    //
    void startup(int restart);
    void draw();
    void draw_status(const std::string &msg);
    void draw_tag();
    void wksp_redraw();

    // Colors
    enum class Color {
        EMPTY = 1,  // Empty line
        STATUS,     // Status line
        POSITION,   // Cursor position
        TRUNCATION, // Truncation
    };
    void start_color(Color pair);
    void end_color(Color pair);

    // Basic editing/input
    //
    // Open file from command line arguments.
    //
    void open_initial(int argc, char **argv);
    void save_file();
    void save_as(const std::string &filename);
    void handle_key_cmd(int ch);
    void handle_key_edit(int ch);
    void move_left();
    void move_right();
    void move_up();
    void move_down();
    void ensure_cursor_visible();

    // --- Segment-based reading skeleton ---
    void model_init();
    bool load_file_segments(const std::string &path);

    // Current line buffer operations (prototype's getlin/putline pattern)
    void get_line(int lno); // load line from workspace into current_line buffer
    void put_line();        // write current_line back to workspace if modified

    // Session state
    void save_state();
    void load_state_if_requested(int restart, int argc, char **argv);

    // Helpers
    int current_line_length() const;
    size_t get_actual_col() const; // Get actual column position in line (basecol + cursor_col)
    void goto_line(int lineNumber);
    bool search_forward(const std::string &needle);
    bool search_next();
    bool search_backward(const std::string &needle);
    bool search_prev();
    int total_lines() const;

    // External filter execution
    bool execute_external_filter(const std::string &command, int start_line, int num_lines);

    // Line operations
    void insertlines(int from, int number);
    void deletelines(int from, int number);
    void splitline(int line, int col);
    void combineline(int line, int col);

    // Alternative workspace operations
    void switch_to_alternative_workspace();
    void create_alternative_workspace();
    bool has_alternative_workspace() const;

    // Help file operations
    bool open_help_file();
    bool create_builtin_help();

    // Enhanced parameter system
    void enter_command_mode();
    void handle_area_selection(int ch);
    bool is_movement_key(int ch) const;

    // Signal handling
    void setup_signal_handlers();
    static void handle_sigint(int sig);
    static void handle_fatal_signal(int sig);
    void check_interrupt();

    // Macros
    void save_macro_position(char name);
    bool goto_macro_position(char name);
    void save_macro_buffer(char name);
    bool paste_macro_buffer(char name);
    bool mdeftag(char tag_name); // define area using tag

    // Journaling
    void journal_write_key(int ch);
    int journal_read_key();

    // Clipboard operations
    void picklines(int startLine, int count);
    void paste(int afterLine, int atCol = 0);
    void pickspaces(int line, int col, int number, int nl);
    void closespaces(int line, int col, int number, int nl);
    void openspaces(int line, int col, int number, int nl);

    // Backend editing operations (testable)
    void edit_backspace();          // Handle backspace operation
    void edit_delete();             // Handle delete operation
    void edit_enter();              // Handle enter/newline operation
    void edit_tab();                // Handle tab insertion
    void edit_insert_char(char ch); // Handle character insertion/overwrite
};

#endif
