#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "editor.h"

// ============================================================================
// External filter execution
// ============================================================================

//
// Execute external command as filter on selected lines.
//
bool Editor::execute_external_filter(const std::string &command, int start_line, int num_lines)
{
    // Validate parameters
    auto total = wksp_->total_line_count();
    if (start_line < 0 || start_line >= total) {
        return false;
    }

    // Limit num_lines to available lines
    int end_line = std::min(start_line + num_lines, total);
    num_lines    = end_line - start_line;

    if (num_lines <= 0) {
        return false;
    }

    put_line();

    // Create temporary files for input and output
    std::string input_file  = "/tmp/v-edit_filter_input_" + std::to_string(getpid());
    std::string output_file = "/tmp/v-edit_filter_output_" + std::to_string(getpid());

    // Write selected lines to input file
    std::ofstream input_stream(input_file);
    if (!input_stream) {
        return false;
    }

    for (int i = start_line; i < end_line; ++i) {
        std::string line = wksp_->read_line(i);
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
    wksp_->delete_contents(start_line, start_line + num_lines - 1);

    // Build segments from the new lines
    auto temp_segments = tempfile_.write_lines_to_temp(new_lines);

    if (!temp_segments.empty()) {
        // Insert the new segments at the deletion point
        wksp_->insert_contents(temp_segments, start_line);
    }

    ensure_cursor_visible();

    // Clean up temporary files
    unlink(input_file.c_str());
    unlink(output_file.c_str());

    return true;
}

// ============================================================================
// Workspace and help operations
// ============================================================================

//
// Switch between main and alternative workspace views.
//
void Editor::switch_to_alternative_workspace()
{
    if (!has_alternative_workspace()) {
        create_alternative_workspace();
    }

    // Swap workspaces
    std::swap(wksp_, alt_wksp_);

    // Swap filename
    std::string temp_filename = filename_;
    filename_                 = alt_filename_;
    alt_filename_             = temp_filename;

    ensure_cursor_visible();
}

//
// Create new alternative workspace.
//
void Editor::create_alternative_workspace()
{
    // Create a fresh alternative workspace (with tempfile reference)
    alt_wksp_     = std::make_unique<Workspace>(tempfile_);
    alt_filename_ = filename_;

    // Try to open the help file in alternative workspace
    if (!open_help_file()) {
        // If help file fails, create a new empty workspace
        alt_filename_ = "untitled_alt";
        alt_wksp_->load_text("");
    }
}

//
// Check if alternative workspace exists.
//
bool Editor::has_alternative_workspace() const
{
    // We have an alternative workspace if we have at least something stored
    return !alt_filename_.empty();
}

//
// Load help file into alternative workspace.
//
bool Editor::open_help_file()
{
    // Try to open the help file
    std::ifstream help_file(DEFAULT_HELP_FILE);
    if (!help_file.is_open()) {
        // If the system help file doesn't exist, create a simple built-in help
        return create_builtin_help();
    }

    // Read the help file content into a single string
    std::string help_content;
    std::string line;
    while (std::getline(help_file, line)) {
        help_content += line + "\n";
    }
    help_file.close();

    if (help_content.empty()) {
        help_content = "\n";
    }

    // Set alternative workspace to help file
    alt_filename_ = DEFAULT_HELP_FILE;
    alt_wksp_     = std::make_unique<Workspace>(tempfile_);
    alt_wksp_->load_text(help_content);

    return true;
}

//
// Create built-in help content if system help unavailable.
//
bool Editor::create_builtin_help()
{
    // Create built-in help content
    const char *help_text =
        "V-EDIT - Minimal Text Editor\n"
        "\n"
        "BASIC COMMANDS:\n"
        "  ^A (F1)     - Enter command mode\n"
        "  ^N          - Switch to alternative workspace/help\n"
        "  F2          - Save file\n"
        "  F3          - Next file\n"
        "  F4          - External filter\n"
        "  F5          - Copy line\n"
        "  F6          - Paste line\n"
        "  F7          - Search\n"
        "  F8          - Go to line\n"
        "\n"
        "COMMAND MODE:\n"
        "  qa          - Quit all\n"
        "  o<file>     - Open file\n"
        "  <number>    - Go to line\n"
        "\n"
        "MOVEMENT:\n"
        "  Arrow keys  - Move cursor\n"
        "  Home/End    - Line start/end\n"
        "  Page Up/Dn  - Page up/down\n"
        "\n"
        "EDITING:\n"
        "  ^D          - Delete character\n"
        "  ^Y          - Delete line\n"
        "  ^C          - Copy line\n"
        "  ^V          - Paste line\n"
        "  ^O          - Insert line\n"
        "\n"
        "Press ^N to return to your file.\n";

    // Set alternative workspace to built-in help
    alt_filename_ = "Built-in Help";
    alt_wksp_     = std::make_unique<Workspace>(tempfile_);
    alt_wksp_->load_text(help_text);

    return true;
}
