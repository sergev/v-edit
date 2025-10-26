# Ve Editor Commands

This document describes all commands available in ve's command mode. Enter command mode by pressing **F1** or **^A**.

## Command Mode Overview

In command mode, you enter a line-based command and terminate it with Enter. Commands can have:
- **No parameters** - e.g., quit, save
- **String parameters** - filenames, search text
- **Numeric parameters** - line numbers, counts
- **Area selection** - rectangular or line ranges defined by cursor movement

## Basic Commands

### File Operations

#### Open File
- **Command**: `o<filename>`
- **Description**: Open a file in the editor
- **Example**: `otest.txt`

#### Save File
- **Command**: `s` or `:w`
- **Description**: Save current file to disk
- **Example**: `s`

#### Save As
- **Command**: `s<filename>` or `:w <filename>`
- **Description**: Save current file with a different name
- **Example**: `sbackup.txt`

#### Quit Without Save
- **Command**: `qa` or `:q`
- **Description**: Exit editor without saving
- **Example**: `qa`

#### Quit With Save
- **Command**: `:wq`
- **Description**: Save and exit
- **Example**: `:wq`

### Navigation

#### Goto Line
- **Command**: `g<number>` or `:<number>`
- **Description**: Jump to specified line number
- **Example**: `g42` (goto line 42)

#### Search Forward
- **Command**: `/text` or `+text`
- **Description**: Search forward for text
- **Example**: `/hello`

#### Search Backward
- **Command**: `?text` or `-text`
- **Description**: Search backward for text
- **Example**: `?hello`

### Editing

#### Copy (Line/Block)
- **Command**: `^C` (with area selection)
- **Description**: Copy selected text to clipboard
- **Usage**: 
  - With cursor movement: Defines area
  - Without movement: Copies current line

#### Paste
- **Command**: `^V`
- **Description**: Paste clipboard at cursor position
- **Usage**: `^V` (no parameters)

#### Delete Line/Block
- **Command**: `^Y` (with area selection)
- **Description**: Delete selected text
- **Usage**: 
  - With cursor movement: Defines area
  - Without movement: Deletes current line

#### Insert Blank Line/Spaces
- **Command**: `^O` (with area selection)
- **Description**: Insert blank line or rectangular block of spaces
- **Usage**: 
  - Line: Just `^O`
  - Block: With cursor movement to define area

## Advanced Commands

### Cursor Movement with Count

All movement commands can be prefixed with a number to repeat the movement:
- **Format**: `<number><movement_key>`
- **Example**: `10↑` moves up 10 lines

Movement keys:
- `↑` - Up
- `↓` - Down
- `←` - Left
- `→` - Right
- Home
- End
- Page Up
- Page Down

### Area Selection

Commands that work on text areas:

1. **Start area selection**: Use any movement key in command mode
2. **Visual feedback**: Message "*** Area defined by cursor ***"
3. **Apply command**: Press the command key (^C, ^Y, ^O)

#### Copy Area
- **Trigger**: Move cursor to define area, then press `^C`

#### Delete Area
- **Trigger**: Move cursor to define area, then press `^Y`

#### Insert Spaces in Area
- **Trigger**: Move cursor to define area, then press `^O`

### Line Operations with Count

Format: `<command><count>`

- **Copy N lines**: `^C<number>` - Copy N lines from current position
- **Delete N lines**: `^Y<number>` - Delete N lines from current position
- **Insert N blank lines**: `^O<number>` - Insert N blank lines

### Viewport Shifting

#### Shift View Right
- **Command**: `<number>→` or `^X f`
- **Description**: Shift display right by columns
- **Example**: `5→` (shift right 5 columns)

#### Shift View Left
- **Command**: `<number>←` or `^X b`
- **Description**: Shift display left by columns
- **Example**: `3←` (shift left 3 columns)

### External Filter Execution

- **Command**: `|command` (in filter mode via F4)
- **Description**: Run shell command on selected lines
- **Example**: `|sort` (sort selected lines)

### Macro Commands

#### Store Position Marker
- **Command**: `>x` (where x is a letter a-z)
- **Description**: Save current cursor position with name
- **Example**: `>a`

#### Go to Position Marker
- **Command**: `$x` (where x is a letter a-z)
- **Description**: Jump to saved position
- **Example**: `$a`

#### Store Copy Buffer
- **Command**: `^C>name`
- **Description**: Store clipboard with name
- **Example**: Move cursor to define area, then press `^C>b`

#### Retrieve Copy Buffer
- **Command**: `^V$name`
- **Description**: Paste named buffer
- **Example**: `^V$b`

#### Define Area Using Marker
- **Command**: `$name` (after moving cursor)
- **Description**: Define text area between current position and marker
- **Usage**: Move cursor, then `$name`

### Special Return Commands

Commands entered with Enter:

#### Redraw Screen
- **Command**: `r`
- **Description**: Refresh the display
- **Example**: `r`

#### Quit
- **Command**: `q`
- **Description**: Exit (same as `qa`)
- **Example**: `q`

#### Abort
- **Command**: `qa`
- **Description**: Immediate quit without save
- **Example**: `qa`

## Command Mode Indicators

When in command mode, the status bar shows "Cmd: " followed by your input.

Area selection mode shows: "*** Area defined by cursor ***"

## Implementation Status

Not all prototype commands are implemented in the current ve C++ version. Implemented commands include:

- Basic file operations (open, save, quit)
- Search (/ and ?)
- Goto line (g, :)
- Area selection and block operations
- Clipboard copy/paste
- Line/block delete and insert
- Some macro support (position markers, named buffers)

## See Also

- [README.md](../README.md) - General editor usage
- [Testing.md](Testing.md) - Test suite documentation
- [AI_Prompt.md](AI_Prompt.md) - Developer documentation
