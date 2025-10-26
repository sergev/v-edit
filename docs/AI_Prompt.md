# Project AI Prompt

This file captures context and conventions for assisting on this repository. Load it before working in this codebase.

## Project at a glance
- **Name**: ve — minimal ncurses-based text editor (C++17)
- **Binary**: `ve` (two letters, simple)
- **Build system**: CMake (>= 3.15)
- **Core sources**: `core.cpp`, `display.cpp`, `input.cpp`, `edit.cpp`, `file.cpp`, `files.cpp`, `clipboard.cpp`, `session.cpp`, `filter.cpp`, `segments.cpp`, `signal_handler.cpp`, `main.cpp`
- **Main header**: `editor.h` — contains the `Editor` class definition
- **Libraries**: ncurses
- **Platform**: macOS primary (AppleClang); portable C++17

## Architecture Overview

### Dual-Mode Data Model
The editor uses a hybrid approach with two data storage mechanisms:

1. **In-Memory Lines** (`std::vector<std::string> lines`):
   - Primary editing interface
   - Fast, accessible for immediate edits
   - Rebuilt into segment chains when needed

2. **Segment Chain Model** (from prototype):
   - Linked list of segments with byte data
   - Efficient for large file handling
   - Allows lazy loading and streaming
   - Used for file I/O and persistence

The editor maintains both representations with a dirty flag system to keep them in sync.

### Key Components
- **Editor class**: Singleton pattern (signal handlers need static instance pointer)
- **Multiple file support**: Tab-like switching between open files
- **Alternative workspace**: `^N` to switch between two independent workspaces
- **Session management**: Automatic save/restore of position and state
- **Journaling**: Keystroke recording for debugging and replay
- **Signal handling**: Graceful crash recovery, interrupt handling

## Core Features

### Editing Modes
- **Insert mode** (default): Characters inserted at cursor
- **Overwrite mode**: Characters replace existing text (toggle with `^X i`)
- **Command mode**: `F1` or `^A` to enter commands

### Editing Operations
- **Character edit**: Insert/overwrite characters, tabs
- **Line operations**: Copy (`^C`), paste (`^V`), delete (`^Y`), insert blank (`^O`)
- **Rectangular block operations**: Copy (`^C` in area selection), delete (`^Y`), insert spaces (`^O`)
- **Clipboard**: Line ranges and rectangular blocks

### File Operations
- **Open**: `o<filename>` or `F3` (next file)
- **Save**: `F2`, `^A s`, or `:w`
- **Exit**: `qa` (no save), `^X ^C` (save and exit), `:wq`

### Navigation
- **Search**: `/text` (forward), `?text` (backward), `n` (next)
- **Goto line**: `g<number>` or `:<number>`
- **Scroll**: `^X f/b` (shift view), Page Up/Down, Home/End

### Advanced Features
- **External filters**: `F4` to run shell commands on selected lines
- **Macros**: Position markers (`>>x`) and buffers (`>x`, use with `$x`)
- **Session restore**: Automatically saves/restores editing position
- **Help file**: Built-in help accessible via `^N` (alternative workspace)

### UI
- **Status bar**: Shows line/column, mode, filename (cyan background, black text if colors supported)
- **Cursor positioning**: Uses `move(row, col)` before input
- **Display refresh**: Manual `refresh()` calls; timeout-based input loop

## Build System

### CMake Structure
```cmake
v_edit_lib (static)  # All sources except main.cpp
    ↓
ve (executable)      # Main entry point
    ↓
v_edit_tests         # Test suite linking v_edit_lib
```

- **Library**: `v_edit_lib` static library with all editor logic
- **Executable**: `ve` links to library
- **Warnings**: `-Wall -Werror -Wshadow` (non-MSVC)
- **Install**: Both `ve` and `v_edit_lib` installed

Typical build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build
```

### Source Files and Purposes

**Core Files**:
- `main.cpp` - Entry point, argument parsing, help messages
- `editor.h` - Main Editor class definition with all data structures
- `core.cpp` - Initialization, model setup, main loop
- `display.cpp` - Drawing, status bar, cursor positioning, color support
- `input.cpp` - Key handling (edit mode, command mode, area selection)
- `edit.cpp` - File operations, search, navigation
- `file.cpp` - File I/O operations
- `files.cpp` - Multiple file management
- `clipboard.cpp` - Clipboard operations
- `session.cpp` - Session save/restore, journaling
- `filter.cpp` - External filter execution
- `segments.cpp` - Segment chain model implementation
- `signal_handler.cpp` - Signal handling, crash recovery

## Tests

### Test Framework
- **GoogleTest** (v1.15.2) via FetchContent
- **Test target**: `v_edit_tests`
- **Fixture**: `TmuxDriver` for ncurses UI testing
- **Isolation**: Dedicated tmux server (`-L v-edit-tests-<pid>`)

### Running Tests
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build
ctest -V
```

### Test Infrastructure
- **TmuxDriver**: Provides `createSession()`, `sendKeys()`, `capturePane()`, `killSession()`
- **Test macro**: `V_EDIT_BIN_PATH` injected by CMake
- **Auto-skip**: If `tmux` not available

## Key Data Structures

### Editor State
```cpp
int cursor_col, cursor_line;           // Cursor position in view
int ncols, nlines;                      // Terminal dimensions
std::vector<std::string> lines;         // In-memory file content
std::string status;                     // Status message
bool cmd_mode;                          // Command mode active
bool insert_mode;                       // Insert vs overwrite
```

### Clipboard
```cpp
struct Clipboard {
    std::vector<std::string> lines;
    int start_line, end_line;
    int start_col, end_col;
    bool is_rectangular;
};
```

### Segment Chain (for large files)
```cpp
struct Segment {
    Segment *prev, *next;
    int nlines, fdesc;
    long seek;
    std::vector<unsigned char> data;
};
```

### Macros
```cpp
struct Macro {
    enum Type { POSITION, BUFFER };
    Type type;
    std::pair<int, int> position;       // For POSITION
    std::vector<std::string> buffer_lines; // For BUFFER
    int start_line, end_line;
    int start_col, end_col;
    bool is_rectangular;
};
```

## Important Patterns

### Main Loop (core.cpp)
```cpp
timeout(200);  // Non-blocking input with 200ms timeout
for (;;) {
    check_interrupt();
    move(cursor_line, cursor_col);  // Position cursor before input
    int ch = journal_read_key();
    if (ch == ERR) {
        draw();  // Redraw even without input
    } else {
        journal_write_key(ch);  // Record
        handle_key(ch);
        draw();
    }
    if (quit_flag) break;
}
```

### Display Refresh
- Every loop iteration calls `draw()`
- `draw()` calls `ensure_segments_up_to_date()` if segments dirty
- Cursor positioned with `move(cursor_line, cursor_col)` before input

### Key Handling Flow
```
handle_key()
  ├─ cmd_mode? → handle_key_cmd()
  │                ├─ area_selection_mode? → handle_area_selection()
  │                └─ Movement keys update param bounds
  └─ handle_key_edit()
     └─ Various editing operations
```

### Color Support (display.cpp)
```cpp
void start_status_color() {
    if (has_colors()) {
        attron(COLOR_PAIR(1));  // Black on cyan
    } else {
        attron(A_REVERSE);
    }
}
```

## Session Management
- **State file**: `~/.ve/session` (via tmpname in `/tmp/ret*` on macOS)
- **Journal**: `/tmp/rej*` records all keystrokes
- **Restore modes**:
  - No args → restore state
  - `-` argument → replay journal

## Important Conventions

### Code Style
- **Indentation**: 4 spaces (C++ and CMake)
- **Formatting**: Run `make reindent` for clang-format
- **Naming**: 
  - Member variables in camelCase
  - Functions in snake_case
  - Structs in PascalCase

### Editor State Management
- **Dirty tracking**: `segments_dirty` flag for lazy rebuilding
- **File state**: Saved before workspace switches
- **Backup tracking**: `backup_done` flag per file

### Signal Handling
- Static instance pointer for signal handlers
- Graceful shutdown with session save
- Interrupt flag for user-canceled operations

## Adding New Features

### UI Operations
1. Add state variables to `Editor` class in `editor.h`
2. Handle key in `input.cpp` → `handle_key_edit()` or `handle_key_cmd()`
3. Update display in `display.cpp` as needed
4. Write test in `tests/` with `TEST_F(TmuxDriver, ...)`
5. Add to `tests/CMakeLists.txt` target `v_edit_tests`

### File Operations
1. Modify `file.cpp` or `files.cpp` for file I/O
2. Update `segments.cpp` if changing segment model
3. Consider session state in `session.cpp`

## Repository Layout (Key Paths)
- `CMakeLists.txt` — root build; defines `v_edit_lib` and `ve`
- `Makefile` — convenience targets (`all`, `install`, `reindent`)
- `core.cpp` — main loop, initialization
- `display.cpp` — rendering, status bar, color
- `input.cpp` — keyboard input handling
- `edit.cpp` — file operations, search, navigation
- `file.cpp` — file I/O
- `files.cpp` — multiple file management
- `clipboard.cpp` — clipboard operations
- `session.cpp` — session management
- `filter.cpp` — external filter execution
- `segments.cpp` — segment chain model
- `signal_handler.cpp` — signal handling
- `editor.h` — main class definition
- `main.cpp` — entry point
- `tests/CMakeLists.txt` — test build config
- `tests/TmuxDriver.h/.cpp` — tmux-based gtest fixture
- `tests/*_test.cpp` — individual tests
- `docs/Testing.md` — testing methodology
- `docs/AI_Prompt.md` — this file
- `README.md` — user documentation

## Known Decisions
- GoogleMock is disabled (`BUILD_GMOCK=OFF`)
- CMake policy CMP0135 set to NEW
- `DOWNLOAD_EXTRACT_TIMESTAMP TRUE` for FetchContent downloads
- Session files use per-user temp directories
- Colors: Black text on cyan background for status bar
- Cursor positioned with `move()` before every input read
