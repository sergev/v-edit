#ifndef EDITOR_H
#define EDITOR_H

#include <map>
#include <string>
#include <vector>

class Editor {
private:
    static Editor *instance_; // For signal handler access

public:
    // --- Core data model types (ported from prototype) ---
    struct Segment {
        Segment *prev{ nullptr };
        Segment *next{ nullptr };
        int nlines{ 0 };
        int fdesc{ 0 };
        long seek{ 0 };
        // In C this is a flexible array member; in C++ we store bytes here.
        std::vector<unsigned char> data;
    };

    struct FileDesc {
        Segment *chain{ nullptr };
        std::string name;
        std::string path; // full path for segment-based reading
        int writable{ 0 };
        int backup_done{ 0 };
        int nlines{ 0 };
        std::string contents; // contiguous storage of file text (lines joined by \n)
    };

    struct Workspace {
        Segment *cursegm{ nullptr };
        int topline{ 0 };
        int offset{ 0 };
        int line{ 0 };
        int segmline{ 0 };
        int wfile{ 0 };
        int cursorcol{ 0 };
        int cursorrow{ 0 };
    };

    Editor();
    int run(int argc, char **argv);

private:
    // Minimal single-window state
    int ncols{};
    int nlines{};
    int cursor_col{};
    int cursor_line{};
    std::string status;
    std::vector<std::string> lines; // temporary backing store for display
    std::string filename{ "untitled" };
    bool cmd_mode{ false };
    bool quit_flag{ false };
    bool backup_done{ false };         // track if backup file has been created
    bool segments_dirty{ false };      // track if segments need rebuilding
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

    // Macros (position markers and text buffers)
    struct Macro {
        enum Type { POSITION, BUFFER };
        Type type;
        std::pair<int, int> position;                 // for POSITION type
        std::vector<std::string> buffer_lines;        // for BUFFER type
        int start_line, end_line, start_col, end_col; // buffer bounds
        bool is_rectangular;

        Macro()
            : type(POSITION), position({ 0, 0 }), start_line(-1), end_line(-1), start_col(-1),
              end_col(-1), is_rectangular(false)
        {
        }
    };
    std::map<char, Macro> macros; // char -> macro data

    // Multiple file support
    struct FileState {
        std::string filename;
        std::vector<std::string> lines;
        Workspace wksp;
        bool modified{ false };
        bool backup_done{ false };

        FileState() : filename("untitled"), wksp{} {}
    };
    std::vector<FileState> open_files;
    int current_file_index{ 0 };
    int alternative_file_index{ -1 }; // -1 means no alternative file

    // Alternative workspace support
    Workspace alt_wksp;
    int alt_file_index{ -1 }; // -1 means no file attached

    // Help file system
    static const std::string DEFAULT_HELP_FILE;

    // Enhanced clipboard (supports line ranges)
    struct Clipboard {
        std::vector<std::string> lines;
        int start_line{ -1 };
        int end_line{ -1 };
        int start_col{ -1 };
        int end_col{ -1 };
        bool is_rectangular{ false };
        bool is_empty() const { return lines.empty(); }
        void clear()
        {
            lines.clear();
            start_line = end_line = start_col = end_col = -1;
            is_rectangular                              = false;
        }
    };
    Clipboard clipboard;

    // Segment/file/workspace tables (single-window, at least 2 entries as per prototype usage)
    std::vector<FileDesc> files;
    Workspace wksp{};

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
    void build_segment_chain_from_lines();
    void build_segment_chain_from_text(const std::string &text);
    bool load_file_segments(const std::string &path);
    int wksp_position(int lno);            // set wksp.cursegm/segmline to segment containing lno
    int wksp_seek(int lno, long &outSeek); // compute byte offset of line lno in file

    // helpers
    int decode_line_len(const Segment *seg, size_t &idx) const; // returns length incl. "\n"
    std::string get_line_from_model(int lno);
    std::string get_line_from_segments(int lno); // get line directly from segment chain
    void update_line_in_segments(int lno,
                                 const std::string &new_content); // update line in segment chain
    void ensure_segments_up_to_date();                            // rebuild segments only if dirty

    // Segment-based file storage
    void load_file_to_segments(const std::string &path);
    void build_segment_chain_from_file(int fd);
    std::string read_line_from_segment(int line_no);
    bool write_segments_to_file(const std::string &path);

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

    // Multiple file operations
    bool open_file(const std::string &file_to_open);
    void switch_to_file(int file_index);
    void switch_to_alternative_file();
    void next_file();
    int get_current_file_index() const;
    int get_file_count() const;
    std::string get_current_filename() const;
    bool is_file_modified(int file_index) const;

    // Helper functions for file state management
    void save_current_file_state();
    void load_current_file_state();

    // Alternative workspace operations
    void switch_to_alternative_workspace();
    void create_alternative_workspace();
    bool has_alternative_workspace() const;
    void save_current_workspace_state();
    void load_alternative_workspace_state();

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
