#ifndef EDITOR_H
#define EDITOR_H

#include <map>
#include <string>

#include "clipboard.h"
#include "macro.h"
#include "segment.h"
#include "workspace.h"

class Editor {
private:
    static Editor *instance_; // For signal handler access

public:
    Editor();
    int run(int argc, char **argv);

private:
    // Minimal single-window state
    int ncols{};
    int nlines{};
    int cursor_col{};
    int cursor_line{};
    std::string status;
    std::string filename{ "untitled" };
    bool cmd_mode{ false };
    bool quit_flag{ false };

    // Current line buffer (prototype's cline pattern)
    std::string current_line;
    int current_line_no{ -1 };
    bool current_line_modified{ false };

    bool filter_mode{ false };         // track if we're in filter command mode
    bool area_selection_mode{ false }; // track if we're in area selection mode
    std::string cmd;

    // Enhanced parameter system (from prototype)
    int param_type{ 0 };              // 0=none, 1=string, -1=area, -2=tag-defined area
    std::string param_str;            // string parameter
    int param_c0{ 0 }, param_r0{ 0 }; // area top-left corner
    int param_c1{ 0 }, param_r1{ 0 }; // area bottom-right corner
    int param_count{ 0 };             // numeric count parameter
    std::string last_search;          // last search needle
    bool last_search_forward{ true };
    std::vector<std::string> clipboard_lines; // simple line clipboard (F5/F6)
    bool quote_next{ false };                 // ^P - quote next character literally
    bool ctrlx_state{ false };                // ^X prefix state
    bool insert_mode{ true };                 // insert vs overwrite mode

    // Signal handling
    bool interrupt_flag{ false }; // interrupt signal occurred

    // Two workspaces: main workspace (wksp) and alternative workspace (alt_wksp)
    Workspace wksp;
    Workspace alt_wksp;
    std::string alt_filename;

    // Help file installed in a public place
    static const std::string DEFAULT_HELP_FILE;

    // Enhanced clipboard (supports line ranges)
    Clipboard clipboard;

    // Macros (position markers and text buffers)
    std::map<char, Macro> macros; // char -> macro data

    // Journaling
    int journal_fd{ -1 };
    std::string jname;
    std::string tmpname;
    std::string rfile;
    int inputfile{ 0 };    // 0=stdin, >=0=journal file fd for replay
    int restart_mode{ 0 }; // 0=normal, 1=restore, 2=replay

    // Startup and display
    //
    // Initialize ncurses terminal.
    //
    void startup(int restart);
    void draw();
    void draw_status(const std::string &msg);
    void wksp_redraw();
    void start_status_color();
    void end_status_color();

    // Basic editing/input
    //
    // Open file from command line arguments.
    //
    void open_initial(int argc, char **argv);
    void save_file();
    void save_as(const std::string &filename);
    void handle_key(int ch);
    void handle_key_cmd(int ch);
    void handle_key_edit(int ch);
    void move_left();
    void move_right();
    void move_up();
    void move_down();
    void ensure_cursor_visible();

    // --- Segment-based reading skeleton ---
    void model_init();
    void build_segment_chain_from_text(const std::string &text);
    bool load_file_segments(const std::string &path);
    bool load_file_to_segments(const std::string &path);

    // Current line buffer operations (prototype's getlin/putline pattern)
    void get_line(int lno);   // load line from workspace into current_line buffer
    void put_line();          // write current_line back to workspace if modified
    void ensure_line_saved(); // flush current line if modified

    // Helper to read line from workspace (replaces get_line_from_model/segs)
    std::string read_line_from_wksp(int lno);

    // Session state
    void save_state();
    void load_state_if_requested(int restart, int argc, char **argv);

    // Helpers
    int current_line_length() const;
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

#ifdef FRIEND_TEST
    friend class SegmentTest; // For segment unit testing
    FRIEND_TEST(SegmentTest, LoadFileToSegments);
    FRIEND_TEST(SegmentTest, ReadLineFromSegment);
    FRIEND_TEST(SegmentTest, HandleEmptyFile);
    FRIEND_TEST(SegmentTest, HandleLargeFile);
    FRIEND_TEST(SegmentTest, HandleVeryLongLines);
    FRIEND_TEST(SegmentTest, WriteSegmentsToFile);
    FRIEND_TEST(SegmentTest, SegmentChainFromVariableLines);
#endif
};

#endif
