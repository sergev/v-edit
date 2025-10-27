#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <fstream>

#include "editor.h"

//
// Parse text and build segment chain from it.
//
void Editor::build_segments_from_text(const std::string &text)
{
    // Build segment chain directly from text in workspace
    wksp->build_segments_from_text(text);
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

    current_line_modified = false;

    // Write the modified line to temp file and get a segment for it
    Segment *new_seg = tempfile_.write_line_to_temp(current_line);
    if (!new_seg) {
        // TODO: error message, as we lost contents of the current line.
        current_line_no = -1;
        return;
    }

    int line_no     = current_line_no;
    current_line_no = -1;

    if (wksp->nlines() <= line_no) {
        // Is it safe to increase nlines before extending the segment chain?
        wksp->set_nlines(line_no + 1);
    }

    // Break segment at line_no position to split into segments before and at line_no
    int break_result = wksp->breaksegm(line_no, true);

    if (break_result == 0) {
        // Normal case: line exists, split it
        // Now cursegm_ points to the segment starting at line_no
        // Get the segment we want to replace (the one containing line_no)
        Segment *old_seg = wksp->cursegm();
        Segment *prev    = old_seg ? old_seg->prev : nullptr;

        // Check if the segment only contains one line (or if we're at end of file)
        int segmline       = wksp->segmline();
        bool only_one_line = (old_seg->nlines == 1);

        Segment *after = nullptr;

        if (only_one_line) {
            // Segment already contains only the one line we want to replace
            // Just get the next segment
            after = old_seg->next;
        } else {
            // Break at line_no + 1 to isolate the line
            int break2_result = wksp->breaksegm(line_no + 1, false);

            if (break2_result == 0) {
                // Now cursegm_ points to segment starting at line_no + 1
                after = wksp->cursegm();
            } else {
                // Second break created blank lines - we're at end of file
                after = old_seg->next;
            }
        }

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
        wksp->set_segmline(segmline);

        // Free the old segment
        delete old_seg;

        // Try to merge adjacent segments (but not if we just created blank lines)
        if (only_one_line || line_no < wksp->nlines() - 1) {
            wksp->catsegm();
        }

        // Mark workspace as modified
        wksp->set_modified(true);
    } else if (break_result == 1) {
        // breaksegm created blank lines - insert the new segment
        // This matches prototype putline() when flg != 0
        Segment *wg = wksp->cursegm();
        int cur_segmline = wksp->segmline();
        
        Segment *w0 = wg ? wg->prev : nullptr;
        Segment *after = wg ? wg->next : nullptr;
        
        // If wg has multiple lines and we're replacing a line in the middle, need to split
        if (wg && wg->nlines > 1 && cur_segmline < line_no) {
            int rel_line = line_no - cur_segmline;
            
            if (rel_line > 0 && rel_line < wg->nlines) {
                // Split wg into: [before] + [at line_no] + [after]
                // Create segment for lines before line_no
                Segment *before_blank = new Segment();
                before_blank->nlines = rel_line;
                before_blank->fdesc = -1;
                before_blank->seek = 0;
                before_blank->sizes.resize(rel_line);
                for (int i = 0; i < rel_line; ++i) {
                    before_blank->sizes[i] = 1;
                }
                
                // Create segment for lines after line_no
                Segment *after_blank = nullptr;
                if (rel_line + 1 < wg->nlines) {
                    after_blank = new Segment();
                    after_blank->nlines = wg->nlines - rel_line - 1;
                    after_blank->fdesc = -1;
                    after_blank->seek = 0;
                    after_blank->sizes.resize(after_blank->nlines);
                    for (int i = 0; i < after_blank->nlines; ++i) {
                        after_blank->sizes[i] = 1;
                    }
                }
                
                // Link: w0 -> before_blank -> new_seg -> after_blank -> after
                if (w0) {
                    w0->next = before_blank;
                }
                before_blank->prev = w0;
                before_blank->next = new_seg;
                
                new_seg->prev = before_blank;
                
                if (after_blank) {
                    new_seg->next = after_blank;
                    after_blank->prev = new_seg;
                    after_blank->next = after;
                    if (after) {
                        after->prev = after_blank;
                    }
                } else {
                    new_seg->next = after;
                    if (after) {
                        after->prev = new_seg;
                    }
                }
                
                // Update chain head if needed
                if (!w0) {
                    wksp->set_chain(before_blank);
                }
                
                delete wg;
                
                // Update workspace position
                wksp->set_cursegm(new_seg);
                wksp->set_segmline(line_no);
                
                wksp->set_modified(true);
                return;
            }
        }
        
        // Simple case: replace the entire blank segment
        delete wg;

        // Link new segment - after should already exist (possibly the tail)
        new_seg->prev = w0;
        new_seg->next = after;

        if (w0) {
            w0->next = new_seg;
        } else {
            wksp->set_chain(new_seg);
        }
        
        if (after) {
            after->prev = new_seg;
        }

        // Update workspace position
        wksp->set_cursegm(new_seg);
        wksp->set_segmline(line_no);

        // Mark workspace as modified
        wksp->set_modified(true);
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
            build_segments_from_text("");
        }
    } else {
        // No file specified, create untitled file with one empty line
        filename = "untitled";
        build_segments_from_text("");
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

    // Ensure we have segments to save
    if (!wksp->has_segments()) {
        status = "No content to save";
        return;
    }

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

    // Use workspace's write_segments_to_file
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
