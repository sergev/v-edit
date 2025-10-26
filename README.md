ve - A lightweight plain text editor with the C/C++ ncurses library.

Usage:
    ve                    # Restore last session
    ve -                  # Replay journal
    ve [file]             # Open file
    ve --help             # Show help
    ve --version          # Show version

Basic Editing:

 * Printable characters - insert into text
 * Cursor left, right, up, down - move cursor
 * Enter - split line or create new line
 * Delete / ^D - delete character under cursor
 * Backspace - erase previous character
 * Tab - insert spaces up to next 4-column boundary
 * Escape - ignore

Line Operations:

 * ^C - copy current line to clipboard
 * ^V - paste clipboard below current line
 * ^Y - delete current line (saves to clipboard)
 * ^O - insert blank line below cursor

File Operations:

 * F2 or ^A s - save file
 * ^X ^C - save file and exit
 * qa (in command mode) - quit without saving

Command Mode (F1):

 * /text - search forward
 * ?text - search backward
 * n - find next match
 * N - find previous match
 * g<number> - goto line number
 * :w - save file (vi-style)
 * :q - quit (vi-style)
 * :wq - save and quit (vi-style)
 * :<number> - goto line (vi-style)
 * >x - save macro position (x = a-z)
 * $x - goto macro position (x = a-z)

Shortcuts:

 * ^F or F7 - search forward dialog
 * ^B - search backward dialog
 * ^O - insert blank line
 * ^P - quote next character (literal insert)
 * F8 - goto line dialog
 * F3 - show help

View Control (^X prefix):

 * ^X f - shift view right
 * ^X b - shift view left
 * ^X i - toggle insert/overwrite mode
 * ^X ^C - save and exit

Mode Display:

 * INSERT mode - characters are inserted (default)
 * OVERWRITE mode - characters replace existing text

Session Management:

 * Last session state (file, position) is automatically saved
 * Journal of keystrokes is recorded for debugging
 * Use `-` argument to replay journal
