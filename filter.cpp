#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
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
    if (start_line < 0 || start_line >= wksp->nlines()) {
        return false;
    }

    // Limit num_lines to available lines
    int end_line = std::min(start_line + num_lines, wksp->nlines());
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
        status = std::string("Filter command failed: ") + full_command;
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

    // Replace the selected lines with the output
    // Delete old lines
    wksp->delete_segments(start_line, start_line + num_lines - 1);

    // Insert new content
    wksp->build_segment_chain_from_text(output_text);

    // Update line count - TODO: need better integration
    int new_num_lines = 0;
    Segment *seg      = wksp->chain();
    while (seg) {
        assert(seg->fdesc != 0); // fdesc should be present for segments with content
        new_num_lines += seg->nlines;
        seg = seg->next;
    }
    wksp->set_nlines(wksp->nlines() - num_lines + new_num_lines);

    ensure_cursor_visible();

    // Clean up temporary files
    unlink(input_file.c_str());
    unlink(output_file.c_str());

    return true;
}
