#include <algorithm>
#include <fstream>
#include <iostream>

#include "editor.h"

// Alternative workspace operations
//
// Switch between main and alternative workspace views.
//
void Editor::switch_to_alternative_workspace()
{
    if (!has_alternative_workspace()) {
        create_alternative_workspace();
    }

    // Swap workspaces
    Workspace temp_wksp = wksp;
    wksp                = alt_wksp;
    alt_wksp            = temp_wksp;

    // Swap filename and lines
    std::string temp_filename = filename;
    filename                  = alt_filename;
    alt_filename              = temp_filename;

    std::vector<std::string> temp_lines = lines;
    lines                               = alt_lines;
    alt_lines                           = temp_lines;

    // Rebuild segments from swapped lines
    build_segment_chain_from_lines();
    ensure_cursor_visible();
}

//
// Create new alternative workspace.
//
void Editor::create_alternative_workspace()
{
    // Save current workspace state to alt_wksp
    alt_wksp     = wksp;
    alt_filename = filename;
    alt_lines    = lines;

    // Try to open the help file in alternative workspace
    if (!open_help_file()) {
        // If help file fails, create a new empty workspace
        alt_filename = "untitled_alt";
        alt_lines.clear();
        alt_lines.push_back("");
        alt_wksp = Workspace{};
    }

    // Rebuild segments for alternative workspace
    alt_wksp.build_segment_chain_from_lines(alt_lines);
}

//
// Check if alternative workspace exists.
//
bool Editor::has_alternative_workspace() const
{
    // We have an alternative workspace if we have at least something stored
    return !alt_filename.empty() || !alt_lines.empty();
}

// Help file operations
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

    // Read the help file content
    std::vector<std::string> help_lines;
    std::string line;
    while (std::getline(help_file, line)) {
        help_lines.push_back(line);
    }
    help_file.close();

    if (help_lines.empty()) {
        help_lines.push_back("");
    }

    // Set alternative workspace to help file
    alt_filename = DEFAULT_HELP_FILE;
    alt_lines    = help_lines;
    alt_wksp     = Workspace{};
    alt_wksp.build_segment_chain_from_lines(help_lines);

    return true;
}

//
// Create built-in help content if system help unavailable.
//
bool Editor::create_builtin_help()
{
    // Create built-in help content
    std::vector<std::string> help_lines = { "V-EDIT - Minimal Text Editor",
                                            "",
                                            "BASIC COMMANDS:",
                                            "  ^A (F1)     - Enter command mode",
                                            "  ^N          - Switch to alternative workspace/help",
                                            "  F2          - Save file",
                                            "  F3          - Next file",
                                            "  F4          - External filter",
                                            "  F5          - Copy line",
                                            "  F6          - Paste line",
                                            "  F7          - Search",
                                            "  F8          - Go to line",
                                            "",
                                            "COMMAND MODE:",
                                            "  qa          - Quit all",
                                            "  o<file>     - Open file",
                                            "  <number>    - Go to line",
                                            "",
                                            "MOVEMENT:",
                                            "  Arrow keys  - Move cursor",
                                            "  Home/End    - Line start/end",
                                            "  Page Up/Dn  - Page up/down",
                                            "",
                                            "EDITING:",
                                            "  ^D          - Delete character",
                                            "  ^Y          - Delete line",
                                            "  ^C          - Copy line",
                                            "  ^V          - Paste line",
                                            "  ^O          - Insert line",
                                            "",
                                            "Press ^N to return to your file." };

    // Set alternative workspace to built-in help
    alt_filename = "Built-in Help";
    alt_lines    = help_lines;
    alt_wksp     = Workspace{};
    alt_wksp.build_segment_chain_from_lines(help_lines);

    return true;
}
