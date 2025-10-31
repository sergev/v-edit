#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <fstream>
#include <list>
#include <ostream>
#include <string>
#include <vector>

#include "segment.h"

// Forward declaration
class Tempfile;

//
// View-related state (display and cursor position)
//
struct ViewState {
    int topline{ 0 };   // top line visible on screen
    int basecol{ 0 };   // horizontal scroll base column
    int cursorcol{ 0 }; // saved cursor column
    int cursorrow{ 0 }; // saved cursor row
};

//
// Position state (navigation within file)
//
struct PositionState {
    int line{ 0 }; // current line number
};

//
// File metadata state
//
struct FileState {
    bool modified{ false };    // track if file has been modified
    bool backup_done{ false }; // track if backup file has been created
    bool writable{ false };    // write permission
};

//
// Workspace class - manages segment list and file workspace state.
// Encapsulates segment list operations and positioning.
//
class Workspace {
public:
    Workspace(Tempfile &tempfile);
    ~Workspace();

    // Public state access via structs
    ViewState view;
    PositionState position;
    FileState file_state;

    //
    // Segment list operations
    //

    // Build list of segments from file descriptor
    // File descriptor is inherited, and closed in destructor
    void load_file(int fd);

    // Build list of segments from in-memory lines vector
    void load_text(const std::vector<std::string> &lines);

    // Build list of segments from text string
    void load_text(const std::string &text);

    // Write segment list content to file
    bool write_file(const std::string &path);

    // Compute total line count of all segments.
    int total_line_count() const;

    // Read line content from segment list at specified index
    std::string read_line(int line_no);

    // Change cursegm_ to the segment containing the specified line
    // Also updates line_ to position the workspace at line number
    // Throws std::runtime_error for invalid line numbers or corrupted contents
    int change_current_line(int lno);

    // Compute the line number of the first line in the current segment
    // by walking backwards from cursegm_ and summing line counts
    int current_segment_base_line() const;

    // Clean up segment list
    void cleanup_contents();

    // Reset workspace state
    void reset();

    // Access to segments list for internal operations
    std::list<Segment> &get_contents() { return contents_; }
    const std::list<Segment> &get_contents() const { return contents_; }

    // Direct iterator access for segment manipulation
    Segment::iterator cursegm() { return cursegm_; }

    //
    // Segment manipulation (from prototype)
    //

    // Write line content back to workspace at specified line number
    // Replaces or inserts the line in the segment chain
    void put_line(int line_no, const std::string &line_content);

    // Insert segments into workspace before given line (insert from prototype)
    void insert_contents(std::list<Segment> &segments, int at);

    // Delete segments from workspace between from and to lines (delete from prototype)
    void delete_contents(int from, int to);

    // Split segment at given line number
    int split(int line_no);

    // Merge adjacent segments
    bool merge();

    // Create segments for n empty lines (blanklines from prototype)
    static std::list<Segment> create_blank_lines(int n);

    //
    // View management methods (from prototype)
    //

    // Go to a specific line in the file (gtfcn from prototype)
    void goto_line(int target_line, int max_rows);

    // Scroll workspace by nl lines (negative for up, positive for down)
    // max_rows: maximum visible rows in display
    // total_lines: total lines in file
    void scroll_vertical(int nl, int max_rows, int total_lines);

    // Shift horizontal view by nc columns (negative for left, positive for right)
    // max_cols: maximum visible columns in display
    void scroll_horizontal(int nc, int max_cols);

    // Update topline when file changes (used by wksp_redraw)
    void update_topline_after_edit(int from, int to, int delta);

    // Debug routine: print all fields and segment list
    void debug_print(std::ostream &out) const;

private:
    // Helper for load_file: parse a single line from buffered input
    // Returns line length including newline, or 0 on EOF
    int parse_line_from_buffer(char *read_buf, int &buf_count, int &buf_next, int fd);

    // Helper for split: handle empty workspace case
    int split_empty_workspace(int line_no);

    // Helper for split: extend file beyond end with blank lines
    int split_beyond_end(int line_no);

    // Helper for split: split segment at relative line position
    void split_segment(int rel_line);

    // Helper for insert_contents: determine insertion point after split
    Segment::iterator determine_insertion_point(int br, int at, int total_before);

    // Helper for delete_contents: handle fast-path deletion of last line
    bool delete_last_line_fastpath();

    // Helper for delete_contents: determine delete range endpoints
    bool determine_delete_range(int from, int to, Segment::iterator &start_it,
                                Segment::iterator &end_it);

    // Helper for delete_contents: update workspace position after deletion
    void update_position_after_deletion(Segment::iterator after_delete_it);

    std::list<Segment> contents_; // list of segments
    Segment::iterator cursegm_;   // current segment iterator (points into contents_)
    Tempfile &tempfile_;          // reference to temp file manager
    int original_fd_{ -1 };       // file descriptor for original file
};

#endif // WORKSPACE_H
