ve - A lightweight plain text editor with the C/C++ ncurses library.

Build and Install:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    make -C build
    make -C build install

Usage:
    ve                    # Restore last session
    ve -                  # Replay journal
    ve [file]             # Open file
    ve --help             # Show help
    ve --version          # Show version

Basic Editing:

 * Printable characters - insert into text
 * Cursor left, right, up, down - move cursor
 * Home/End - jump to start/end of line
 * Page Up/Down - scroll page up/down
 * Enter - split line or create new line
 * Delete / ^D - delete character under cursor
 * Backspace - erase previous character
 * Tab - insert spaces up to next 4-column boundary
 * Escape - cancel operation

Line Operations (Edit Mode):

 * ^C - copy current line to clipboard
 * ^V - paste clipboard below current line
 * ^Y - delete current line (saves to clipboard)
 * ^O - insert blank line below cursor

Function Keys:

 * F1 or ^A - enter command mode
 * F2 - save file
 * F3 - switch to next file
 * F4 - execute external filter
 * F5 - copy current line
 * F6 - paste clipboard
 * F7 - search forward dialog (same as ^F)
 * F8 - goto line dialog

Shortcuts (Edit Mode):

 * ^C - copy current line to clipboard
 * ^V - paste clipboard
 * ^Y - delete current line
 * ^O - insert blank line
 * ^D - delete character under cursor
 * ^P - quote next character (literal insert)
 * ^N - switch to alternative workspace (opens help file)
 * ^F or F7 - search forward dialog
 * ^B - search backward dialog
 * ^X prefix - extended commands (see below)

View Control (^X prefix):

 * ^X f - shift view right
 * ^X b - shift view left
 * ^X i - toggle insert/overwrite mode
 * ^X ^C - save file and exit

Command Mode (F1 or ^A):

Search and Navigation:
 * /text or +text - search forward
 * ?text or -text - search backward
 * n - find next match
 * N - find previous match
 * g<number> or :<number> - goto line number

File Operations:
 * o<filename> - open file
 * s or :w - save file
 * s<filename> - save as
 * :q or qa - quit without saving
 * :wq - save and quit

Area Selection (Rectangular Blocks):
 * Move cursor to define rectangular area
 * ^C - copy rectangular block
 * ^Y - delete rectangular block
 * ^O - insert rectangular block of spaces
 * <number><movement> - move with count
 * <number>→ or ← - shift view with count

Line Operations with Count:
 * ^C<number> - copy N lines
 * ^Y<number> - delete N lines
 * ^O<number> - insert N blank lines

Macros:
 * >x - save position marker (x = a-z)
 * $x - goto position marker
 * ^C>x - save buffer with name
 * ^V$x - paste named buffer

Other Commands:
 * r - redraw screen
 * |command - execute shell filter on selection
 * Press ESC, F1, or ^A to cancel selection

Mode Display:

 * INSERT mode - characters are inserted (default)
 * OVERWRITE mode - characters replace existing text

Advanced Features:

Multiple Files:
 * F3 - switch to next open file
 * o<filename> - open file in editor
 * Multiple files can be open simultaneously
 * Each file maintains its own cursor position

Alternative Workspace:
 * ^N - switch between two independent workspaces
 * First press creates help file in alternative workspace
 * Subsequent presses toggle between workspaces
 * Useful for keeping reference material open

External Filters:
 * F4 - enter filter command mode
 * |command - pipe selected lines through shell command
 * Example: |sort sorts selected lines

Session Management:

 * Last session state (file, position) is automatically saved
 * Journal of keystrokes is recorded for debugging
 * Use `-` argument to replay journal
 * Session persists across crashes

See Also:
 * [Commands](docs/Commands.md) - Complete command reference
 * [Rectangular Blocks](docs/Rectangular_Blocks.md) - Operations with rectangular blocks
