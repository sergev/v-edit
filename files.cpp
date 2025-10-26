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
    std::swap(wksp, alt_wksp);

    // Swap filename
    std::string temp_filename = filename;
    filename                  = alt_filename;
    alt_filename              = temp_filename;

    ensure_cursor_visible();
}

//
// Create new alternative workspace.
//
void Editor::create_alternative_workspace()
{
    // Create a fresh alternative workspace (with tempfile reference)
    alt_wksp     = std::make_unique<Workspace>(tempfile_);
    alt_filename = filename;

    // Try to open the help file in alternative workspace
    if (!open_help_file()) {
        // If help file fails, create a new empty workspace
        alt_filename = "untitled_alt";
        alt_wksp->build_segment_chain_from_text("");
    }
}

//
// Check if alternative workspace exists.
//
bool Editor::has_alternative_workspace() const
{
    // We have an alternative workspace if we have at least something stored
    return !alt_filename.empty();
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
    alt_filename = DEFAULT_HELP_FILE;
    alt_wksp     = std::make_unique<Workspace>(tempfile_);
    alt_wksp->build_segment_chain_from_text(help_content);

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
    alt_filename = "Built-in Help";
    alt_wksp     = std::make_unique<Workspace>(tempfile_);
    alt_wksp->build_segment_chain_from_text(help_text);

    return true;
}
