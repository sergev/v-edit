#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#include "editor.h"

//
// Load line from workspace into current line buffer.
//
void Editor::get_line(int lno)
{
    current_line_modified_ = false;
    current_line_no_       = lno;

    current_line_ = wksp_->read_line(lno);
}

//
// Write current line buffer back to workspace if modified.
//
void Editor::put_line()
{
    if (!current_line_modified_ || current_line_no_ < 0) {
        current_line_no_ = -1;
        return;
    }

    wksp_->put_line(current_line_no_, current_line_);
    current_line_modified_ = false;
    current_line_no_       = -1;
}

//
// Ensure current line is saved before operations.
//
void Editor::ensure_line_saved()
{
    if (current_line_modified_ && current_line_no_ >= 0) {
        put_line();
    }
}

//
// Open initial file from command line arguments.
//
void Editor::open_initial(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] != '-') {
        // Open the specified file
        filename_ = argv[1];
        if (!load_file_segments(filename_)) {
            // File doesn't exist or can't be opened - create empty segments for new file
            wksp_->load_text("");
        }
    } else {
        // No file specified, create untitled file with one empty line
        filename_ = "untitled";
        wksp_->load_text("");
    }
    status_ = "Cmd: ";
}

//
// Load file content into workspace.
//
bool Editor::load_file_segments(const std::string &path)
{
    // Open file for reading
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        status_ = std::string("Cannot open file: ") + path;
        return false;
    }

    // Use workspace's load_file_to_segments to properly set up segments.
    wksp_->load_file(fd);

    // Note: we keep the fd open because segments reference it via file_descriptor
    return true;
}

//
// Write current buffer to file and create backup.
//
void Editor::save_file()
{
    ensure_line_saved(); // Save any unsaved line modifications

    // Create backup file if not already done and file exists
    if (!wksp_->file_state.backup_done && filename_ != "untitled") {
        std::string backup_name = filename_ + "~";
        // Remove existing backup
        unlink(backup_name.c_str());
        // Create hard link to original file BEFORE we unlink it
        if (link(filename_.c_str(), backup_name.c_str()) < 0 && errno != ENOENT) {
            // Backup failed, but continue with save
            status_ = std::string("Backup failed, continuing save");
        } else {
            wksp_->file_state.backup_done = true;
        }
    }

    // Unlink the original file to ensure backup is not affected by the write
    // NOTE: The original fd remains valid even after unlinking because we created a hard link
    if (filename_ != "untitled") {
        unlink(filename_.c_str());
    }

    // Use workspace's write_file
    bool saved = wksp_->write_file(filename_);
    if (saved) {
        status_                    = std::string("Saved: ") + filename_;
        wksp_->file_state.modified = false; // Clear modified flag after save
    } else {
        status_ = std::string("Cannot write: ") + filename_;
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

    // Use workspace's write_file
    if (wksp_->write_file(new_filename)) {
        // Update filename
        filename_ = new_filename;
        status_   = std::string("Saved as: ") + new_filename;
    } else {
        status_ = std::string("Cannot write: ") + new_filename;
    }
}
