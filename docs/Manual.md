# ve Editor Manual

A comprehensive guide to using ve, a minimal ncurses-based text editor with advanced editing capabilities.

## Introduction

ve is a terminal-based text editor designed for efficient text editing with support for multiple files, rectangular block operations, macros, and session management. The editor operates in full-screen mode and provides a command-based interface similar to classic text editors.

## Starting the Editor

### Basic Usage

To start ve, use one of the following commands:

```bash
ve                    # Restore last session or create new empty file
ve <file>             # Open a specific file
ve <file> <line>      # Open file and jump to specified line number
ve -R                 # Replay keystrokes from journal file
```

### Session Restoration

The editor automatically saves your session state (cursor position, open files) to `~/.ve/session`. When you restart ve without arguments, it restores your previous editing session, allowing you to continue where you left off.

### Journal Replay

ve records all keystrokes to journal files (`/tmp/rej{tty}{user}`) for debugging and crash recovery. Use `ve -R` to replay a previous editing session.

## Editing Modes

ve operates in several distinct modes, each optimized for different tasks:

### Edit Mode

The default mode for text editing. In edit mode:
- Characters are inserted at the cursor position
- Arrow keys move the cursor
- Printable characters are typed directly into the file
- Press `F1` or `^A` to enter command mode

### Command Mode

Entered by pressing `F1` or `^A`. In command mode:
- You type commands on the status line
- Commands are terminated with Enter
- Movement keys can define rectangular areas for operations
- Press Escape or Enter (on empty command) to return to edit mode

### Insert vs Overwrite Mode

By default, ve operates in insert mode where typed characters are inserted at the cursor. You can toggle overwrite mode using `^X i`, which causes characters to replace existing text instead of inserting.

### Filter Mode

Entered by pressing `F4`. Allows you to execute external shell commands (filters) on selected lines. Type a pipe symbol followed by the command, e.g., `|sort`.

## Basic Text Editing

### Character Input

In edit mode, printable characters are inserted directly. Special handling exists for:
- **Tab**: Inserts 4 spaces
- **Enter**: Splits the current line at the cursor position
- **Backspace**: Deletes the character before the cursor, joining lines if at the beginning
- **^D**: Deletes the character under the cursor
- **^P**: Quote mode - inserts the next character literally, even if it's a control character

### Cursor Movement

- **Arrow keys**: Move cursor in four directions
- **Home/End**: Move to beginning/end of current line
- **Page Up/Down**: Scroll one page up or down
- **Ctrl+Home/End**: Jump to first or last line of file

At window borders, the viewport automatically scrolls to keep the cursor visible (unless auto-scroll is disabled).

### Line Operations

These commands work directly in edit mode:

- **^C** or **F5**: Copy current line to clipboard
- **^V** or **F6**: Paste clipboard below current line
- **^Y**: Delete current line (saves to clipboard)
- **^O**: Insert blank line below cursor

### Character Deletion

- **Backspace** (or `^H`): Delete character before cursor
- **^D**: Delete character under cursor
- Both operations join lines if deletion occurs at a line boundary

## Navigation and Search

### Going to a Specific Line

You can jump to any line in the file using several methods:

1. **Command mode**: Type `g<number>` or `:<number>` and press Enter
   - Example: `g42` jumps to line 42
   - `g` alone jumps to the beginning of the file
2. **Edit mode**: Press **F8** for a goto line dialog

### Search

ve provides powerful search functionality:

#### Forward Search
- **Command mode**: Type `/text` or `+text` and press Enter
- **Edit mode**: Press `^F` or **F7** for search dialog
- After searching, press `n` to find the next match, `N` for previous

#### Backward Search
- **Command mode**: Type `?text` or `-text` and press Enter
- **Edit mode**: Press `^B` for search dialog
- After searching, press `n` to continue backward, `N` to reverse direction

#### Search Navigation
- **n**: Find next match (forward)
- **N**: Find previous match (backward)

The last search term is remembered, so you can use `n` and `N` to navigate between matches without re-entering the search pattern.

### Viewport Scrolling

When working with long lines or wide files, you can shift the viewport horizontally:

- **^X f**: Shift view right (scroll right)
- **^X b**: Shift view left (scroll left)

The editor automatically scrolls horizontally when the cursor reaches the right edge during typing.

## File Operations

### Opening Files

- **Command mode**: Type `o<filename>` and press Enter
  - Example: `otest.txt` opens `test.txt`
  - If the file doesn't exist, you'll be prompted to create it
- **Command line**: `ve <file>` opens a file when starting the editor

### Saving Files

- **F2**: Save current file
- **Command mode**: Type `s` to save, or `s<filename>` to save with a new name
  - Example: `sbackup.txt` saves the file as `backup.txt`

### Exiting the Editor

- **Command mode**: Type `q` to save and exit, or `qa` to exit without saving
- **Edit mode**: `^X ^C` saves and exits immediately

### Multiple File Editing

ve supports editing multiple files through workspace switching:

- **F3**: Switch to the next workspace/file
- Each workspace maintains independent cursor position and state
- Use `o<filename>` in command mode to open additional files

## Advanced Editing Operations

### Rectangular Block Operations

ve excels at rectangular block editing, which allows you to operate on columns of text independently of line boundaries.

#### Defining a Rectangular Area

1. Press `F1` or `^A` to enter command mode
2. Move the cursor to define the area:
   - Start position is where you enter command mode
   - End position is where the cursor currently is
3. The status bar shows "*** Area defined by cursor ***" when an area is active

The area is defined as the smallest rectangle containing both the start and end cursor positions.

#### Copying Rectangular Blocks

1. Define a rectangular area using cursor movement in command mode
2. Press `^C` to copy the block
3. Move cursor to destination
4. Press `^V` to paste

#### Deleting Rectangular Blocks

1. Define the area to delete
2. Press `^Y` to delete the block
3. The deleted content is saved to the clipboard for pasting

#### Inserting Spaces in Rectangular Areas

1. Define the rectangular area
2. Press `^O` to insert spaces
3. This is useful for indenting columns of text or creating alignment

### Line Range Operations

You can operate on multiple lines at once:

- **^C<number>**: Copy N lines starting from current line
- **^Y<number>**: Delete N lines starting from current line
- **^O<number>**: Insert N blank lines

Example: `^C5` copies 5 lines, `^Y3` deletes 3 lines.

### Clipboard System

ve maintains a clipboard for both line-based and rectangular block operations:

- **Line clipboard**: Used by `^C`/`^V` (F5/F6) for simple line operations
- **Block clipboard**: Maintains rectangular block data with coordinates
- Clipboard contents persist across editor sessions

## Macro System

The macro system allows you to save positions and text buffers for quick recall.

### Position Markers

Save and recall cursor positions:

- **`>x`**: Save current cursor position with name `x` (where `x` is a letter a-z)
- **`$x`**: Jump to position marker `x`
- **`g$x`**: Go to line containing marker `x`

Example workflow:
1. Move to an important location
2. Press `F1`, type `>a`, press Enter
3. Navigate elsewhere
4. Press `F1`, type `$a`, press Enter to return

### Named Buffers

Store text in named buffers for later use:

- **`^C>name`**: Store current clipboard in named buffer
- **`^V$name`**: Paste contents of named buffer

Example:
1. Copy some text (`^C` or define area and `^C`)
2. Press `F1`, type `^C>b`, press Enter (stores in buffer 'b')
3. Later, press `F1`, type `^V$b`, press Enter to paste buffer 'b'

### Using Markers for Area Selection

You can define areas using position markers:

1. Save a position: `>start`
2. Move cursor to end position
3. Use the marker in an area operation: `$start` as part of a command defines the area from the marker to current position

## Alternative Workspace

ve provides a dual-workspace model for working with multiple files or reference material:

- **^N**: Toggle to alternative workspace
- **F3**: Switch between primary and alternative workspaces
- Each workspace maintains independent state (cursor position, file, etc.)
- The alternative workspace initially opens the built-in help file
- Use `o<filename>` in the alternative workspace to open a different file

This is useful for:
- Viewing documentation while editing
- Comparing two files side-by-side
- Keeping reference material accessible

## External Filter Commands

ve can execute external shell commands (filters) on selected text:

1. Select lines using area selection (move cursor in command mode)
2. Press **F4** to enter filter mode
3. Type `|command` and press Enter
   - Example: `|sort` sorts the selected lines
   - Example: `|grep pattern` filters lines
   - The command receives selected lines on stdin and stdout replaces them

Useful filter commands:
- `|sort` - Sort lines alphabetically
- `|uniq` - Remove duplicate consecutive lines
- `|grep pattern` - Filter lines matching pattern
- `|sed 's/old/new/'` - Text substitution
- Any command that reads stdin and writes to stdout

## Function Keys Reference

### Edit Mode Function Keys

- **F1** or **^A**: Enter command mode
- **F2**: Save current file
- **F3**: Switch to next workspace/file
- **F4**: Enter filter command mode
- **F5**: Copy current line (`^C` equivalent)
- **F6**: Paste clipboard (`^V` equivalent)
- **F7**: Search forward dialog (`^F` equivalent)
- **F8**: Goto line dialog

### Edit Mode Control Key Shortcuts

- **^A**: Enter command mode
- **^C**: Copy current line
- **^D**: Delete character under cursor
- **^F**: Search forward dialog
- **^B**: Search backward dialog
- **^N**: Switch to alternative workspace
- **^O**: Insert blank line
- **^P**: Quote next character (literal insert)
- **^V**: Paste clipboard
- **^X i**: Toggle insert/overwrite mode
- **^X f**: Shift view right
- **^X b**: Shift view left
- **^X ^C**: Save and exit
- **^Y**: Delete current line

## Status Bar and Indicators

The bottom line of the screen serves as the status bar, displaying:

- **File information**: Current filename and line number
- **Command mode**: Shows "Cmd: " followed by your input
- **Area selection**: Shows "*** Area defined by cursor ***" when selecting
- **Filter mode**: Shows "Filter command: " when entering filter commands
- **Messages**: Error messages, confirmations, and status updates

## Session Management

### Automatic Session Saving

ve automatically saves your editing session when you exit normally:
- Cursor position
- Current file and workspace
- Viewport position
- Open files

### Crash Recovery

If the editor crashes or the system goes down:
- Your keystrokes are recorded in journal files
- Use `ve -R` to replay the journal and recover your work
- Session state is saved periodically

## Tips and Best Practices

### Efficient Navigation

- Use position markers (`>x`) to mark important locations in large files
- Use search (`/text`, `?text`) instead of scrolling for finding content
- Use `g<number>` to jump directly to specific lines

### Working with Large Files

- ve uses efficient segment-based storage for large files
- Only the current line is kept in memory during editing
- The editor handles files of any size efficiently

### Rectangular Block Editing

- Rectangular blocks are powerful for columnar data manipulation
- Use them for indenting code blocks, aligning columns, or operating on specific columns
- Remember that blocks are defined by cursor positions, so precise placement matters

### Multiple File Workflows

- Use the alternative workspace (`^N`) for reference material
- Switch between files with `F3`
- Each workspace remembers its state independently

### Macro Workflows

- Use position markers to quickly jump between related sections
- Store frequently used text snippets in named buffers
- Combine markers and buffers for complex editing workflows

## Troubleshooting

### Screen Corruption

If the display becomes corrupted, press `^L` or enter command mode and type `r` to redraw the screen.

### Lost Changes

- ve saves changes to a temporary file during editing
- Normal exit (`q`) saves all changes
- Use `qa` only when you want to discard changes
- Journal files allow recovery via `ve -R`

### Command Not Working

- Ensure you're in the correct mode (command mode for commands, edit mode for typing)
- Check the status bar for error messages
- Some commands require area selection - ensure you've defined an area first

## See Also

- [Commands.md](Commands.md) - Detailed command reference
- [Rectangular_Blocks.md](Rectangular_Blocks.md) - Guide to rectangular block operations
- [README.md](../README.md) - Installation and quick start
- [AI_Prompt.md](AI_Prompt.md) - Developer documentation