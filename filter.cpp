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
    if (start_line < 0 || start_line >= (int)lines.size()) {
        return false;
    }

    // Limit num_lines to available lines
    int end_line = std::min(start_line + num_lines, (int)lines.size());
    num_lines    = end_line - start_line;

    if (num_lines <= 0) {
        return false;
    }

    // Create temporary files for input and output
    std::string input_file  = "/tmp/v-edit_filter_input_" + std::to_string(getpid());
    std::string output_file = "/tmp/v-edit_filter_output_" + std::to_string(getpid());

    // Write selected lines to input file
    std::ofstream input_stream(input_file);
    if (!input_stream) {
        return false;
    }

    for (int i = start_line; i < end_line; ++i) {
        input_stream << lines[i];
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

    std::vector<std::string> new_lines;
    std::string line;
    while (std::getline(output_stream, line)) {
        new_lines.push_back(line);
    }

    // If no output, add empty line
    if (new_lines.empty()) {
        new_lines.push_back("");
    }

    // Replace the selected lines with the output
    lines.erase(lines.begin() + start_line, lines.begin() + end_line);
    lines.insert(lines.begin() + start_line, new_lines.begin(), new_lines.end());

    // Update segments
    build_segment_chain_from_lines();
    ensure_cursor_visible();

    // Clean up temporary files
    unlink(input_file.c_str());
    unlink(output_file.c_str());

    return true;
}
