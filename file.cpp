#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>

#include "editor.h"

//
// Rebuild segment chain from in-memory lines vector.
//
void Editor::build_segment_chain_from_lines()
{
    if (files.empty())
        model_init();
    FileDesc &f = files[wksp.wfile];
    f.name      = filename;
    f.writable  = 1;
    f.nlines    = (int)lines.size();
    // Build contiguous contents to facilitate extracting lines by offset
    f.contents.clear();
    for (size_t i = 0; i < lines.size(); ++i) {
        f.contents += lines[i];
        if (i + 1 != lines.size())
            f.contents.push_back('\n');
    }

    // Create actual segment and initialize workspace pointer
    if (f.chain) {
        // Clean up old chain if any
        Segment *seg = f.chain;
        while (seg) {
            Segment *next = seg->next;
            delete seg;
            seg = next;
        }
    }

    Segment *seg = new Segment();
    seg->prev    = nullptr;
    seg->next    = nullptr;
    seg->nlines  = (int)lines.size();
    seg->fdesc   = 0;
    seg->seek    = 0;

    // Build segment chain with line length data
    seg->data.reserve(lines.size() * 2);
    for (const std::string &ln : lines) {
        int n = (int)ln.size() + 1; // include newline
        seg->add_line_length(n);
    }
    f.chain = seg;

    // Set workspace pointer to first segment
    wksp.cursegm  = seg;
    wksp.segmline = 0;

    // Don't reset wksp.topline here as it's called during editing and would reset scroll position
    // Only reset offset and line
    wksp.offset = 0;
    wksp.line   = 0;
}

//
// Parse text and build segment chain from it.
//
void Editor::build_segment_chain_from_text(const std::string &text)
{
    // Split by '\n' into lines and build chain
    lines.clear();
    std::string cur;
    lines.reserve(64);
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    // In case text doesn't end with newline, keep the last line
    if (!cur.empty() || lines.empty())
        lines.push_back(cur);
    build_segment_chain_from_lines();
}

//
// Set workspace to segment containing specified line.
//
int Editor::wksp_position(int lno)
{
    if (!wksp.cursegm)
        return 1;
    Segment *seg = wksp.cursegm;
    int segStart = wksp.segmline;
    // adjust forward
    while (lno >= segStart + seg->nlines && seg->fdesc) {
        segStart += seg->nlines;
        seg = seg->next;
    }
    // adjust backward
    while (lno < segStart && seg->prev) {
        seg = seg->prev;
        segStart -= seg->nlines;
    }
    wksp.cursegm  = seg;
    wksp.segmline = segStart;
    wksp.line     = lno;
    return 0;
}

//
// Compute byte offset of specified line in file.
//
int Editor::wksp_seek(int lno, long &outSeek)
{
    if (wksp_position(lno))
        return 1;
    Segment *seg = wksp.cursegm;
    long seek    = (long)seg->seek;
    int rel      = lno - wksp.segmline;
    size_t idx   = 0;
    for (int i = 0; i < rel; ++i) {
        int len = seg->decode_line_len(idx);
        seek += len;
    }
    outSeek = seek;
    return 0;
}

//
// Retrieve line text from lines vector.
//
std::string Editor::get_line_from_model(int lno)
{
    // For now, fallback to lines vector
    if (lno >= 0 && lno < (int)lines.size())
        return lines[lno];
    return std::string();
}

//
// Retrieve line text from segment chain.
//
std::string Editor::get_line_from_segments(int lno)
{
    // Try segment-based reading first
    if (files.empty() || !files[wksp.wfile].chain) {
        return get_line_from_model(lno); // fallback to lines vector
    }

    // Check if we have file-based segments
    Segment *seg = files[wksp.wfile].chain;
    if (seg && seg->fdesc > 0) {
        // Use segment-based reading
        return read_line_from_segment(lno);
    }

    // Fallback to lines vector
    return get_line_from_model(lno);
}

//
// Update line content in segment chain.
//
void Editor::update_line_in_segments(int lno, const std::string &new_content)
{
    // For now, update the lines vector and rebuild segments
    // In a full implementation, we'd update the segment chain directly
    if (lno >= 0 && lno < (int)lines.size()) {
        lines[lno] = new_content;
    } else if (lno == (int)lines.size()) {
        lines.push_back(new_content);
    }
    build_segment_chain_from_lines();
}

//
// Rebuild segments if they are marked as modified.
//
void Editor::ensure_segments_up_to_date()
{
    if (segments_dirty) {
        build_segment_chain_from_lines();
        segments_dirty = false;
    }
}

//
// Open initial file from command line arguments.
//
void Editor::open_initial(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] != '-') {
        // Open the specified file as the first file
        std::string file_to_open = argv[1];
        open_file(file_to_open);
    } else {
        // No file specified, create untitled file
        FileState untitled_file;
        untitled_file.filename = "untitled";
        untitled_file.lines.push_back("");
        open_files.push_back(untitled_file);
        current_file_index = 0;
        load_current_file_state();
    }
    status = "Cmd: ";
    build_segment_chain_from_lines();
}

//
// Load file content into segment chain.
//
bool Editor::load_file_segments(const std::string &path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in)
        return false;
    std::string text;
    in.seekg(0, std::ios::end);
    std::streampos sz = in.tellg();
    if (sz > 0) {
        text.resize((size_t)sz);
        in.seekg(0, std::ios::beg);
        in.read(&text[0], sz);
    }
    build_segment_chain_from_text(text);
    return true;
}

//
// Write current buffer to file and create backup.
//
void Editor::save_file()
{
    // Create backup file if not already done and file exists
    if (!backup_done && filename != "untitled") {
        std::string backup_name = filename + "~";
        // Remove existing backup
        unlink(backup_name.c_str());
        // Create hard link to original file
        if (link(filename.c_str(), backup_name.c_str()) < 0 && errno != ENOENT) {
            // Backup failed, but continue with save
            status = std::string("Backup failed, continuing save");
        } else {
            backup_done = true;
        }
    }

    // Unlink the original file to ensure backup is not affected by the write
    if (filename != "untitled") {
        unlink(filename.c_str());
    }

    std::ofstream out(filename.c_str());
    if (!out) {
        status = std::string("Cannot write: ") + filename;
        return;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 != lines.size())
            out << '\n';
    }
    out.flush();
    // Ensure data hits disk for test stability
    int fd = ::open(filename.c_str(), O_WRONLY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
    status = std::string("Saved: ") + filename;
}

//
// Save buffer to specified filename.
//
void Editor::save_as(const std::string &new_filename)
{
    // Unlink the original file to ensure backup is not affected by the write
    unlink(new_filename.c_str());

    std::ofstream out(new_filename.c_str());
    if (!out) {
        status = std::string("Cannot write: ") + new_filename;
        return;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 != lines.size())
            out << '\n';
    }
    out.flush();
    // Ensure data hits disk for test stability
    int fd = ::open(new_filename.c_str(), O_WRONLY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }

    // Update filename
    filename = new_filename;
    status   = std::string("Saved as: ") + new_filename;
}
