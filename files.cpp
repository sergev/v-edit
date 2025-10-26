#include <algorithm>
#include <fstream>
#include <iostream>

#include "editor.h"

bool Editor::open_file(const std::string &file_to_open)
{
    // Check if file is already open
    for (size_t i = 0; i < open_files.size(); ++i) {
        if (open_files[i].filename == file_to_open) {
            switch_to_file(i);
            return true;
        }
    }

    // Save current file state before switching
    save_current_file_state();

    // Create new file state
    FileState new_file;
    new_file.filename = file_to_open;

    // Try to load the file
    std::ifstream in(file_to_open.c_str());
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            new_file.lines.push_back(line);
        }
        if (new_file.lines.empty()) {
            new_file.lines.push_back("");
        }
    } else {
        // File doesn't exist, create empty file
        new_file.lines.push_back("");
    }

    // Add to open files
    open_files.push_back(new_file);
    current_file_index = open_files.size() - 1;

    // Update current state
    load_current_file_state();

    return true;
}

void Editor::switch_to_file(int file_index)
{
    if (file_index < 0 || file_index >= (int)open_files.size()) {
        return;
    }

    // Save current file state
    save_current_file_state();

    // Switch to new file
    current_file_index = file_index;

    // Load new file state
    load_current_file_state();
}

void Editor::switch_to_alternative_file()
{
    if (alternative_file_index >= 0 && alternative_file_index < (int)open_files.size()) {
        // Switch to alternative file
        int temp               = current_file_index;
        current_file_index     = alternative_file_index;
        alternative_file_index = temp;

        // Save and load states
        save_current_file_state();
        load_current_file_state();
    } else {
        // No alternative file, create new untitled file
        open_file("untitled");
        alternative_file_index = current_file_index;
    }
}

void Editor::next_file()
{
    if (open_files.size() <= 1) {
        return; // No other files to switch to
    }

    int next_index = (current_file_index + 1) % open_files.size();
    switch_to_file(next_index);
}

int Editor::get_current_file_index() const
{
    return current_file_index;
}

int Editor::get_file_count() const
{
    return open_files.size();
}

std::string Editor::get_current_filename() const
{
    if (current_file_index >= 0 && current_file_index < (int)open_files.size()) {
        return open_files[current_file_index].filename;
    }
    return "untitled";
}

bool Editor::is_file_modified(int file_index) const
{
    if (file_index >= 0 && file_index < (int)open_files.size()) {
        return open_files[file_index].modified;
    }
    return false;
}

void Editor::save_current_file_state()
{
    if (current_file_index >= 0 && current_file_index < (int)open_files.size()) {
        FileState &file  = open_files[current_file_index];
        file.filename    = filename;
        file.lines       = lines;
        file.wksp        = wksp;
        file.backup_done = backup_done;
        file.modified    = true; // Mark as modified when we save state
    }
}

void Editor::load_current_file_state()
{
    if (current_file_index >= 0 && current_file_index < (int)open_files.size()) {
        const FileState &file = open_files[current_file_index];
        filename              = file.filename;
        lines                 = file.lines;
        wksp                  = file.wksp;
        backup_done           = file.backup_done;

        // Update segments
        build_segment_chain_from_lines();
        ensure_cursor_visible();
    }
}

// Alternative workspace operations
void Editor::switch_to_alternative_workspace()
{
    if (!has_alternative_workspace()) {
        create_alternative_workspace();
    }

    // We now definitely have an alternative workspace
    // But we need to also save the current file state before switching
    save_current_file_state();

    // Save current workspace state
    save_current_workspace_state();

    // Swap workspaces
    Workspace temp_wksp = wksp;
    wksp                = alt_wksp;
    alt_wksp            = temp_wksp;

    // Swap file indices
    int temp_file_index = current_file_index;
    current_file_index  = alt_file_index;
    alt_file_index      = temp_file_index;

    // Load the new current file state (which is now the alternative workspace)
    load_current_file_state();
}

void Editor::create_alternative_workspace()
{
    // Save current file state first
    save_current_file_state();

    // Save current workspace state
    save_current_workspace_state();

    // Initialize alternative workspace with current workspace state
    alt_wksp = wksp;

    // Try to open the help file first
    if (open_help_file()) {
        // Help file was created successfully, alt_file_index is now set
        return;
    }

    // If help file fails, create a new untitled file for the alternative workspace
    FileState new_file;
    new_file.filename = "untitled_alt";
    new_file.lines.push_back("");

    // Add to open files
    open_files.push_back(new_file);
    alt_file_index = open_files.size() - 1;
}

bool Editor::has_alternative_workspace() const
{
    return alt_file_index >= 0;
}

void Editor::save_current_workspace_state()
{
    if (current_file_index >= 0 && current_file_index < (int)open_files.size()) {
        FileState &file = open_files[current_file_index];
        file.wksp       = wksp;
    }
}

void Editor::load_alternative_workspace_state()
{
    if (alt_file_index >= 0 && alt_file_index < (int)open_files.size()) {
        const FileState &file = open_files[alt_file_index];
        filename              = file.filename;
        lines                 = file.lines;
        backup_done           = file.backup_done;

        // Update segments
        build_segment_chain_from_lines();
        ensure_cursor_visible();
    }
}

// Help file operations
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

    // Create a help file state
    FileState help_file_state;
    help_file_state.filename   = DEFAULT_HELP_FILE;
    help_file_state.lines      = help_lines;
    help_file_state.wksp       = Workspace{};
    help_file_state.wksp.wfile = 1; // Use temp file slot

    // Add to open files
    open_files.push_back(help_file_state);
    alt_file_index = open_files.size() - 1;

    return true;
}

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

    // Create a help file state
    FileState help_file_state;
    help_file_state.filename   = "Built-in Help";
    help_file_state.lines      = help_lines;
    help_file_state.wksp       = Workspace{};
    help_file_state.wksp.wfile = 1; // Use temp file slot

    // Add to open files
    open_files.push_back(help_file_state);
    alt_file_index = open_files.size() - 1;

    return true;
}
