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
    wksp->build_segment_chain_from_text(text);
}

//
// Load line from workspace into current line buffer.
//
void Editor::get_line(int lno)
{
    current_line_modified = false;
    current_line_no       = lno;

    if (!wksp->has_segments()) {
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

    if (wksp->nlines() <= current_line_no) {
        wksp->set_nlines(current_line_no + 1);
    }

    current_line_modified = false;

    // Write the modified line to temp file and get a segment for it
    Segment *new_seg = wksp->write_line_to_temp(current_line);
    if (!new_seg) {
        current_line_no = -1;
        return;
    }

    int line_no     = current_line_no;
    current_line_no = -1;

    // Break segment at line_no position to split into segments before and at line_no
    if (wksp->breaksegm(line_no, true) == 0) {
        // Now cursegm_ points to the segment starting at line_no
        // Get the segment we want to replace (the one containing line_no)
        Segment *old_seg = wksp->cursegm();
        Segment *prev    = old_seg ? old_seg->prev : nullptr;

        // Break at line_no + 1 to isolate the line
        if (wksp->breaksegm(line_no + 1, false) == 0) {
            // Now cursegm_ points to segment starting at line_no + 1
            Segment *after = wksp->cursegm();

            // old_seg is the segment containing ONLY line_no (isolated between prev and after)

            // Link new segment in place of old_seg
            new_seg->prev = prev;
            new_seg->next = after;

            if (prev) {
                prev->next = new_seg;
            } else {
                wksp->set_chain(new_seg);
            }

            if (after) {
                after->prev = new_seg;
            }

            // Update workspace position
            wksp->set_cursegm(new_seg);
            wksp->set_segmline(line_no);

            // Free the old isolated segment
            delete old_seg;

            // Try to merge adjacent segments
            wksp->catsegm();

            // Mark workspace as modified
            wksp->set_modified(true);
        }
    }
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
    if (!wksp->has_segments()) {
        return std::string();
    }

    // Use workspace's read_line_from_segment
    return wksp->read_line_from_segment(lno);
}

//
// Open initial file from command line arguments.
//
void Editor::open_initial(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] != '-') {
        // Open the specified file
        filename = argv[1];
        if (!load_file_segments(filename)) {
            // File doesn't exist or can't be opened - create empty segments for new file
            build_segment_chain_from_text("");
        }
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
    // Use workspace's load_file_to_segments to properly set up path and segments
    return load_file_to_segments(path);
}

//
// Load file content into workspace's segment chain.
//
bool Editor::load_file_to_segments(const std::string &path)
{
    wksp->load_file_to_segments(path);
    if (!wksp->chain()) {
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
    ensure_line_saved(); // Save any unsaved line modifications

    // Create backup file if not already done and file exists
    if (!wksp->backup_done() && filename != "untitled") {
        std::string backup_name = filename + "~";
        // Remove existing backup
        unlink(backup_name.c_str());
        // Create hard link to original file BEFORE we unlink it
        if (link(filename.c_str(), backup_name.c_str()) < 0 && errno != ENOENT) {
            // Backup failed, but continue with save
            status = std::string("Backup failed, continuing save");
        } else {
            wksp->set_backup_done(true);
        }
    }

    // Unlink the original file to ensure backup is not affected by the write
    // NOTE: The original fd remains valid even after unlinking because we created a hard link
    if (filename != "untitled") {
        unlink(filename.c_str());
    }

    // Ensure we have segments to save
    if (!wksp->has_segments()) {
        status = "No content to save";
        return;
    }

    // Use workspace's write_segments_to_file
    // Note: original file has been unlinked, so unchanged segments won't be readable
    // But modified segments (in temp file) will be written
    bool saved = wksp->write_segments_to_file(filename);
    if (saved) {
        status = std::string("Saved: ") + filename;
        wksp->set_modified(false); // Clear modified flag after save
    } else {
        status = std::string("Cannot write: ") + filename;
    }
}

//
// Save buffer to specified filename.
//
void Editor::save_as(const std::string &new_filename)
{
    ensure_line_saved(); // Save any unsaved line modifications

    // Unlink the original file to ensure backup is not affected by the write
    unlink(new_filename.c_str());

    // Use workspace's write_segments_to_file
    if (wksp->write_segments_to_file(new_filename)) {
        // Update filename
        filename = new_filename;
        status   = std::string("Saved as: ") + new_filename;
    } else {
        status = std::string("Cannot write: ") + new_filename;
    }
}
