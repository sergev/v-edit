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
    int line{ 0 };     // current line number
    int segmline{ 0 }; // first line in current segment
};

//
// File metadata state
//
struct FileState {
    bool modified{ false };    // track if file has been modified
    bool backup_done{ false }; // track if backup file has been created
    int writable{ 0 };         // write permission
    int nlines{ 0 };           // line count
};

//
// Workspace class - manages segment chain and file workspace state.
// Encapsulates segment chain operations and positioning.
//
class Workspace {
public:
    Workspace(Tempfile &tempfile);
    ~Workspace();

    // Public state access via structs
    ViewState view;
    PositionState position;
    FileState file_state;

    // Advanced: Direct iterator access for segment manipulation
    // Note: These methods expose internal iterators for efficient std::list operations
    // Use with caution - prefer higher-level methods when available
    std::list<Segment>::const_iterator cursegm() const { return cursegm_; }
    std::list<Segment>::iterator cursegm() { return cursegm_; }

    // Iterator-based setter (enhanced for modern C++)
    void set_cursegm(std::list<Segment>::iterator it) { cursegm_ = it; }

    // Segment chain operations
    // Build segment chain from in-memory lines vector
    void load_text(const std::vector<std::string> &lines);

    // Build segment chain from text string
    void load_text(const std::string &text);

    // Change current segment to the segment containing the specified line
    // Updates cursegm_, segmline_, and line_ to position the workspace at line number
    // Throws std::runtime_error for invalid line numbers or corrupted segment chain
    int change_current_line(int lno);

    // Load file content into segment chain structure
    void load_file(const std::string &path, bool create_if_missing = true);

    // Build segment chain from file descriptor
    void load_file(int fd);

    // Read line content from segment chain at specified index
    std::string read_line_from_segment(int line_no);

    // Write segment chain content to file
    bool write_segments_to_file(const std::string &path);

    // Clean up segment chain
    void cleanup_segments();

    // Reset workspace state
    void reset();

    // Query methods
    bool has_segments() const { return !segments_.empty(); }
    Tempfile &get_tempfile() const { return tempfile_; }

    // Access to segments list for internal operations
    std::list<Segment> &get_segments() { return segments_; }
    const std::list<Segment> &get_segments() const { return segments_; }

    // Line count
    int get_line_count(int fallback_count) const
    {
        return segments_.empty() ? fallback_count : file_state.nlines;
    }

    // Segment chain management (for backward compatibility during transition)
    void set_chain(std::list<Segment> &segments);

    // Segment manipulation (from prototype)
    // Split segment at given line number (breaksegm from prototype)
    int breaksegm(int line_no, bool realloc_flag = true);

    // Merge adjacent segments (catsegm from prototype)
    bool catsegm();

    // Insert segments into workspace before given line (insert from prototype)
    void insert_segments(std::list<Segment> &segments, int at);

    // Delete segments from workspace between from and to lines (delete from prototype)
    void delete_segments(int from, int to);

    // Copy segment list (copysegm from prototype)
    static std::list<Segment> copy_segment_list(Segment::iterator start, Segment::iterator end);

    // Create segments for n empty lines (blanklines from prototype)
    static std::list<Segment> create_blank_lines(int n);

    // Cleanup a segment list (static helper)
    static void cleanup_segments(std::list<Segment> &segments);

    // View management methods (from prototype)
    // Scroll workspace by nl lines (negative for up, positive for down)
    // max_rows: maximum visible rows in display
    // total_lines: total lines in file
    void scroll_vertical(int nl, int max_rows, int total_lines);

    // Shift horizontal view by nc columns (negative for left, positive for right)
    // max_cols: maximum visible columns in display
    void scroll_horizontal(int nc, int max_cols);

    // Go to a specific line in the file (gtfcn from prototype)
    void goto_line(int target_line, int max_rows);

    // Update topline when file changes (used by wksp_redraw)
    void update_topline_after_edit(int from, int to, int delta);

    // Debug routine: print all fields and segment chain
    void debug_print(std::ostream &out) const;

private:
    Tempfile &tempfile_;          // reference to temp file manager
    std::list<Segment> segments_; // list-based segment chain
    Segment::iterator cursegm_;   // current segment iterator (points into segments_)
    int original_fd_{ -1 };       // file descriptor for original file
};

#endif // WORKSPACE_H
