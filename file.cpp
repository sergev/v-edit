#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>

#include "editor.h"

//
// Parse text and build segment chain from it.
//
void Editor::build_segment_chain_from_text(const std::string &text)
{
    // Build segment chain directly from text in workspace
    wksp.build_segment_chain_from_text(text);
}

//
// Load line from workspace into current line buffer.
//
void Editor::get_line(int lno)
{
    current_line_modified = false;
    current_line_no        = lno;
    
    // Check cache first
    auto it = line_cache.find(lno);
    if (it != line_cache.end()) {
        current_line = it->second;
        return;
    }
    
    if (!wksp.has_segments()) {
        if (current_line.empty()) {
            current_line = "";
        } else {
            current_line.resize(0);
        }
        return;
    }
    
    current_line = read_line_from_wksp(lno);
}

//
// Write current line buffer back to workspace if modified.
//
void Editor::put_line()
{
    if (!current_line_modified || current_line_no < 0) {
        current_line_no = -1;
        return;
    }
    
    // Cache the modified line
    line_cache[current_line_no] = current_line;
    
    current_line_modified = false;
    current_line_no        = -1;
}

//
// Ensure current line is saved before operations.
//
void Editor::ensure_line_saved()
{
    if (current_line_modified && current_line_no >= 0) {
        put_line();
    }
}

//
// Read line from workspace segments.
//
std::string Editor::read_line_from_wksp(int lno)
{
    // Check cache first
    auto it = line_cache.find(lno);
    if (it != line_cache.end()) {
        return it->second;
    }
    
    if (!wksp.has_segments()) {
        return std::string();
    }
    
    // Use workspace's read_line_from_segment
    return wksp.read_line_from_segment(lno);
}

//
// Open initial file from command line arguments.
//
void Editor::open_initial(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] != '-') {
        // Open the specified file
        filename = argv[1];
        load_file_segments(filename);
    } else {
        // No file specified, create untitled file with one empty line
        filename = "untitled";
        build_segment_chain_from_text("");
    }
    status = "Cmd: ";
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
// Load file content into workspace's segment chain.
//
bool Editor::load_file_to_segments(const std::string &path)
{
    wksp.load_file_to_segments(path);
    if (!wksp.chain()) {
        status = std::string("Cannot open file: ") + path;
        return false;
    }
    return true;
}

//
// Write current buffer to file and create backup.
//
void Editor::save_file()
{
    ensure_line_saved();  // Save any unsaved line modifications
    
    // Create backup file if not already done and file exists
    if (!wksp.backup_done() && filename != "untitled") {
        std::string backup_name = filename + "~";
        // Remove existing backup
        unlink(backup_name.c_str());
        // Create hard link to original file
        if (link(filename.c_str(), backup_name.c_str()) < 0 && errno != ENOENT) {
            // Backup failed, but continue with save
            status = std::string("Backup failed, continuing save");
        } else {
            wksp.set_backup_done(true);
        }
    }

    // Unlink the original file to ensure backup is not affected by the write
    if (filename != "untitled") {
        unlink(filename.c_str());
    }

    // Use workspace's write_segments_to_file
    if (wksp.write_segments_to_file(filename)) {
        status = std::string("Saved: ") + filename;
    } else {
        status = std::string("Cannot write: ") + filename;
    }
}

//
// Save buffer to specified filename.
//
void Editor::save_as(const std::string &new_filename)
{
    ensure_line_saved();  // Save any unsaved line modifications
    
    // Unlink the original file to ensure backup is not affected by the write
    unlink(new_filename.c_str());

    // Use workspace's write_segments_to_file
    if (wksp.write_segments_to_file(new_filename)) {
        // Update filename
        filename = new_filename;
        status   = std::string("Saved as: ") + new_filename;
    } else {
        status = std::string("Cannot write: ") + new_filename;
    }
}
