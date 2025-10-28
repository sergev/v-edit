#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <list>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

#include "segment.h"

// Forward declaration
class Tempfile;

//
// Workspace class - manages segment chain and file workspace state.
// Encapsulates segment chain operations and positioning.
//
class Workspace {
public:
    Workspace(Tempfile &tempfile);
    ~Workspace();

    // Accessors
    Segment *chain() const { return segments_.empty() ? nullptr : const_cast<Segment*>(&segments_.front()); }
    Segment *cursegm() const { return cursegm_ == segments_.end() ? nullptr : &*cursegm_; }
    int writable() const { return writable_; }
    int nlines() const { return nlines_; }
    int topline() const { return topline_; }
    int basecol() const { return basecol_; }
    int line() const { return line_; }
    int segmline() const { return segmline_; }
    int cursorcol() const { return cursorcol_; }
    int cursorrow() const { return cursorrow_; }

    // Mutators
    void set_writable(int writable) { writable_ = writable; }
    void set_nlines(int nlines) { nlines_ = nlines; }
    void set_topline(int topline) { topline_ = topline; }
    void set_basecol(int basecol) { basecol_ = basecol; }
    void set_line(int line) { line_ = line; }
    void set_segmline(int segmline) { segmline_ = segmline; }
    void set_cursorcol(int cursorcol) { cursorcol_ = cursorcol; }
    void set_cursorrow(int cursorrow) { cursorrow_ = cursorrow; }

    void set_cursegm(Segment *seg) {
        // Find the iterator for this segment in the list
        for (auto it = segments_.begin(); it != segments_.end(); ++it) {
            if (&*it == seg) {
                cursegm_ = it;
                return;
            }
        }
        // If not found, this is an error - segment not in our list
        cursegm_ = segments_.end();
    }

    // Segment chain operations
    // Build segment chain from in-memory lines vector
    void build_segments_from_lines(const std::vector<std::string> &lines);

    // Build segment chain from text string
    void build_segments_from_text(const std::string &text);

    // Set current segment to the segment containing the specified line
    // Updates cursegm_, segmline_, and line_ to position the workspace at line number
    // Throws std::runtime_error for invalid line numbers or corrupted segment chain
    int set_current_segment(int lno);

    // Load file content into segment chain structure
    void load_file_to_segments(const std::string &path);

    // Build segment chain from file descriptor
    void build_segments_from_file(int fd);

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
    int get_line_count(int fallback_count) const { return segments_.empty() ? fallback_count : nlines_; }

    // File state tracking
    bool modified() const { return modified_; }
    void set_modified(bool modified) { modified_ = modified; }
    bool backup_done() const { return backup_done_; }
    void set_backup_done(bool backup_done) { backup_done_ = backup_done; }

    // Segment chain management (for backward compatibility during transition)
    void set_chain(Segment *chain);

    // Segment manipulation (from prototype)
    // Split segment at given line number (breaksegm from prototype)
    int breaksegm(int line_no, bool realloc_flag = true);

    // Merge adjacent segments (catsegm from prototype)
    bool catsegm();

    // Insert segments into workspace before given line (insert from prototype)
    void insert_segments(Segment *seg, int at);

    // Delete segments from workspace between from and to lines (delete from prototype)
    Segment *delete_segments(int from, int to);

    // Copy segment chain (copysegm from prototype)
    static Segment *copy_segment_chain(Segment *start, Segment *end = nullptr);

    // Create segments for n empty lines (blanklines from prototype)
    static Segment *create_blank_lines(int n);

    // Cleanup a segment chain (static helper)
    static void cleanup_segments(Segment *seg);

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
    int writable_{ 0 };           // write permission
    int nlines_{ 0 };             // line count
    int topline_{ 0 };            // top line visible on screen
    int basecol_{ 0 };            // horizontal scroll base column
    int line_{ 0 };               // current line number
    int segmline_{ 0 };           // first line in current segment
    int cursorcol_{ 0 };          // saved cursor column
    int cursorrow_{ 0 };          // saved cursor row
    bool modified_{ false };      // track if file has been modified
    bool backup_done_{ false };   // track if backup file has been created
    int original_fd_{ -1 };       // file descriptor for original file

    // Helper to update current segment iterator
    void update_current_segment(std::list<Segment>::iterator it) { cursegm_ = it; }
};

#endif // WORKSPACE_H
