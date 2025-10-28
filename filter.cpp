#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "editor.h"

//
// Execute external command as filter on selected lines.
//
bool Editor::execute_external_filter(const std::string &command, int start_line, int num_lines)
{
    // Validate parameters
    if (start_line < 0 || start_line >= wksp_->nlines()) {
        return false;
    }

    // Limit num_lines to available lines
    int end_line = std::min(start_line + num_lines, wksp_->nlines());
    num_lines    = end_line - start_line;

    if (num_lines <= 0) {
        return false;
    }

    ensure_line_saved();

    // Create temporary files for input and output
    std::string input_file  = "/tmp/v-edit_filter_input_" + std::to_string(getpid());
    std::string output_file = "/tmp/v-edit_filter_output_" + std::to_string(getpid());

    // Write selected lines to input file
    std::ofstream input_stream(input_file);
    if (!input_stream) {
        return false;
    }

    for (int i = start_line; i < end_line; ++i) {
        std::string line = read_line_from_wksp(i);
        input_stream << line;
        if (i < end_line - 1) {
            input_stream << '\n';
        }
    }
    input_stream.close();

    // Build command with input/output redirection
    std::string full_command = command + " < " + input_file + " > " + output_file;

    // Execute the command
    int result = system(full_command.c_str());

    // Check if command executed successfully
    if (result != 0) {
        // Debug: print the command that failed
        status_ = std::string("Filter command failed: ") + full_command;
        unlink(input_file.c_str());
        unlink(output_file.c_str());
        return false;
    }

    // Read the output
    std::ifstream output_stream(output_file);
    if (!output_stream) {
        unlink(input_file.c_str());
        unlink(output_file.c_str());
        return false;
    }

    std::string output_text;
    std::string line;
    while (std::getline(output_stream, line)) {
        output_text += line + "\n";
    }

    // If no output, add empty line
    if (output_text.empty()) {
        output_text = "\n";
    }

    // Convert output text to lines vector
    std::vector<std::string> new_lines;
    std::string new_line;
    std::istringstream iss(output_text);
    while (std::getline(iss, new_line)) {
        new_lines.push_back(new_line);
    }

    if (new_lines.empty()) {
        new_lines.push_back(""); // Ensure at least one line
    }

    // Delete old lines
    wksp_->delete_segments(start_line, start_line + num_lines - 1);

    // Build segments from the new lines
    auto temp_segments = tempfile_.write_lines_to_temp(new_lines);

    if (!temp_segments.empty()) {
        // For backward compatibility, create a pointer chain from the list segments
        // First, add all segments to the workspace list
        auto insert_pos = wksp_->get_segments().end();
        for (auto &seg : temp_segments) {
            insert_pos = wksp_->get_segments().insert(insert_pos, std::move(seg));
            ++insert_pos;
        }
        temp_segments.clear(); // moved

        // Now get the first inserted segment as a pointer
        auto first_inserted = wksp_->get_segments().end();
        --first_inserted; // last inserted
        Segment *new_segs = &*first_inserted;

        // Insert the new segments at the deletion point (already in list)
        wksp_->insert_segments(new_segs, start_line);

        // Update line count
        wksp_->set_nlines(wksp_->nlines() - num_lines + (int)new_lines.size());
    }

    ensure_cursor_visible();

    // Clean up temporary files
    unlink(input_file.c_str());
    unlink(output_file.c_str());

    return true;
}
