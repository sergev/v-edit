#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <fstream>
#include <string>
#include <vector>

#include "segment.h"

//
// Workspace class - manages segment chain and file workspace state.
// Encapsulates segment chain operations and positioning.
//
class Workspace {
public:
    Workspace();
    ~Workspace();

    // Copy constructor and assignment (needed for FileState)
    Workspace(const Workspace &other);
    Workspace &operator=(const Workspace &other);

    // Accessors
    Segment *chain() const { return chain_; }
    Segment *cursegm() const { return cursegm_; }
    const std::string &path() const { return path_; }
    int writable() const { return writable_; }
    int nlines() const { return nlines_; }
    int topline() const { return topline_; }
    int basecol() const { return basecol_; }
    int line() const { return line_; }
    int segmline() const { return segmline_; }
    int cursorcol() const { return cursorcol_; }
    int cursorrow() const { return cursorrow_; }

    // Mutators
    void set_path(const std::string &path) { path_ = path; }
    void set_writable(int writable) { writable_ = writable; }
    void set_nlines(int nlines) { nlines_ = nlines; }
    void set_topline(int topline) { topline_ = topline; }
    void set_basecol(int basecol) { basecol_ = basecol; }
    void set_line(int line) { line_ = line; }
    void set_segmline(int segmline) { segmline_ = segmline; }
    void set_cursorcol(int cursorcol) { cursorcol_ = cursorcol; }
    void set_cursorrow(int cursorrow) { cursorrow_ = cursorrow; }

    // Segment chain operations
    // Build segment chain from in-memory lines vector
    void build_segment_chain_from_lines(const std::vector<std::string> &lines);

    // Build segment chain from text string
    void build_segment_chain_from_text(const std::string &text);

    // Position workspace to segment containing specified line
    int position(int lno);

    // Compute byte offset of specified line in file
    int seek(int lno, long &outSeek);

    // Load file content into segment chain structure
    void load_file_to_segments(const std::string &path);

    // Build segment chain from file descriptor
    void build_segment_chain_from_file(int fd);

    // Read line content from segment chain at specified index
    std::string read_line_from_segment(int line_no);

    // Write segment chain content to file
    bool write_segments_to_file(const std::string &path);

    // Clean up segment chain
    void cleanup_segments();

    // Reset workspace state
    void reset();

    // Query methods
    bool has_segments() const { return chain_ != nullptr; }
    bool is_file_based() const;
    bool has_file_path() const { return !path_.empty(); }

    // Line count
    int get_line_count(int fallback_count) const;

private:
    Segment *cursegm_{ nullptr }; // current segment in chain
    Segment *chain_{ nullptr };   // file's segment chain (direct access)
    std::string path_;            // file path (for segment-based I/O)
    int writable_{ 0 };           // write permission
    int nlines_{ 0 };             // line count
    int topline_{ 0 };            // top line visible on screen
    int basecol_{ 0 };            // horizontal scroll base column
    int line_{ 0 };               // current line number
    int segmline_{ 0 };           // first line in current segment
    int cursorcol_{ 0 };          // saved cursor column
    int cursorrow_{ 0 };          // saved cursor row

    // Helper to update current segment pointer
    void update_current_segment(Segment *seg) { cursegm_ = seg; }
};

#endif // WORKSPACE_H
