# Project AI Prompt

This file captures comprehensive context and conventions for assisting on this repository. This is the detailed reference guide.

**Note**: For Cursor IDE users, see `.cursorrules` in the project root for quick reference and mandatory TDD requirements.

⚠️ **IMPORTANT**: All development MUST follow Test-Driven Development (TDD). Write tests first, then implement.

## Project at a glance
- **Name**: ve — minimal ncurses-based text editor (C++17)
- **Binary**: `ve` (two letters, simple)
- **Build system**: CMake (>= 3.15) with version 0.1.0
- **Core sources**: `core.cpp`, `display.cpp`, `input.cpp`, `edit.cpp`, `file.cpp`, `files.cpp`, `clipboard.cpp`, `copy_paste.cpp`, `session.cpp`, `filter.cpp`, `segments.cpp`, `signal.cpp`, `macro.cpp`, `main.cpp`
- **Main header**: `editor.h` — contains the `Editor` class definition
- **Libraries**: ncurses, CMakeFetchContent (GoogleTest)
- **Platform**: macOS primary (AppleClang); portable C++17

## Architecture Overview

### Dual-Workspace Model with Shared Segment Chains
The editor uses a dual-workspace approach with two independent editing contexts:

1. **Primary Workspace** (`wksp_`): Main editing workspace for file content
2. **Alternative Workspace** (`alt_wksp_`): Accessible via `^N` and `F3` for reference, help, or secondary editing

Both workspaces share the same tempfile for efficient memory usage and use the dual data model approach.

### Dual-Mode Data Model
The editor uses two data storage mechanisms within each workspace:

1. **Segment Chain Model** (for large file handling):
   - Linked list of `Segment` objects containing metadata (no raw byte data in memory)
   - **File descriptor**: Points to original file, temp file, or represents empty lines (-1)
   - **Line lengths array**: Stores byte length of each line including newline
   - **File offset**: Position in file where segment data begins
   - Efficient for large file handling with on-demand loading
   - Used for file I/O and persistence operations

2. **In-Memory Current Line** (`std::string current_line_`):
   - Primary editing interface in workspace
   - Fast, accessible for immediate edits
   - Data read from segment (via file I/O) when line loaded with `get_line()`
   - Data written back to temp file when saved with `put_line()`

### Current Line Buffer Pattern
- **During editing**: Use `get_line(line_no)` to load line into `current_line_` buffer
- **After editing**: Call `put_line()` to write buffer back to segment chain if modified
- **State tracking**: `current_line_no_`, `current_line_modified_` flags

### Component Architecture
- **Editor class**: Singleton pattern (signal handlers need static instance pointer)
- **Multiple workspace support**: Tab-like switching between primary and alternative workspaces
- **Session management**: Automatic save/restore of position and state
- **Journaling**: Keystroke recording for debugging and replay
- **Signal handling**: Graceful crash recovery, interrupt handling

## Core Features

### Editing Modes
- **Insert mode** (default): Characters inserted at cursor (`insert_mode_` = true)
- **Overwrite mode**: Characters replace existing text (^X i to toggle)
- **Command mode**: `F1` or `^A` to enter commands
- **Area selection mode**: For selecting rectangular blocks (^C/^Y/^O in selection)
- **Filter mode**: `F4` to execute external shell commands

### Editing Operations

#### Character edit:
- Insert/overwrite characters, tabs
- Tab inserts 4 spaces
- Backspace/delete with line joining
- Enter for line splitting, with text carriage

#### Line operations:
- **Copy** (^C, F5): Copy current line to clipboard
- **Paste** (^V, F6): Paste clipboard below current line
- **Delete** (^Y): Delete current line (saves to clipboard)
- **Insert blank** (^O): Insert blank line below cursor

#### Rectangular block operations:
- Move cursor to define rectangular area (^C/^Y/^O + movement)
- **Copy** (^C): Copy rectangular block
- **Delete** (^Y): Delete rectangular block + save to buffer
- **Insert spaces** (^O): Insert rectangular spaces
- **Line counts**: ^C<number>, ^Y<number>, ^O<number> for multi-line operations

#### Clipboard system:
- **Line-based**: Traditional line copy/paste (F5/F6)
- **Rectangular**: Block-based clipboard with coordinate tracking
- **Serialization**: Clipboard contents persist across sessions

### File Operations
- **Open**: `o<filename>` — opens in current workspace
- **Save**: `F2`, `^A s`, `:w`, `^X ^C` (save and exit)
- **Save as**: `s<filename>`
- **Exit**: `qa` (quit without save), `:q`, `^X ^C`
- **Alternative workspace**: `^N` to switch between workspaces
- **Help file**: Built-in help accessible in alternative workspace

### Navigation & Search
- **Search**: `/text` (forward), `?text` (backward), `n` (next), `N` (previous)
- **Search dialogs**: `^F` or `F7` (forward), `^B` (backward)
- **Goto line**: `g<number>` or `:<number>` or direct `<number>`
- **Scroll**: `^X f/b` (shift view), Page Up/Down, Home/End
- **Cursor movement**: Arrow keys, Home/End for line navigation
- **View control**: `^X f` (right), `^X b` (left) for horizontal scroll

### Advanced Features
- **External filters**: `F4` to run shell commands on selected lines
- **Macros**: Position markers (`>x`) and text buffers (`>>x`) with `$x` recall
- **Session restore**: Automatic state persistence (`~/.ve/session`)
- **Journaling**: Keystroke logs (`/tmp/rej{tty}{user}`) for replay `-` argument
- **Quote mode**: `^P` to insert literal characters
- **Overwrite toggle**: `^X i`

### Function Keys (Edit Mode)
- **F1** (`^A`): Enter command mode
- **F2**: Save file
- **F3**: Switch to next workspace
- **F4**: Filter mode (external commands)
- **F5** (`^C`): Copy current line
- **F6** (`^V`): Paste clipboard
- **F7** (`^F`): Search forward dialog
- **F8** (`g`): Goto line dialog

### Command Mode Operations
- **Line operations**: ^C (copy), ^Y (delete), ^O (insert) with optional counts
- **Area selection**: Movement keys define rectangular bounds for operations
- **Search/navigation**: / ? n N g<number> <number> :wq :q qa
- **File ops**: o s w qa :wq etc. ^C>name ^V$name for macro buffers
- **Macros**: >x (position), ^C>x (buffer), $x (recall position), ^V$x (paste buffer)
- **Filters**: |command (external shell commands)

### Area Selection Mode
- **Activation**: Move cursor after entering command mode
- **Visual feedback**: "*** Area defined by cursor ***" message
- **Operations**: ^C (copy block), ^Y (delete block), ^O (insert spaces)
- **Termination**: Press operation key or ESC/F1 to cancel

### UI Elements
- **Status bar**: Cyan background, black text (shows line/col, mode, filename)
- **Cursor positioning**: `move(row, col)` before input; timeout-based loop
- **Display refresh**: Every input cycle calls `draw()` with `ensure_cursor_visible()`
- **Color support**: Black text on cyan for status (configurable)

## Data Structures

### Editor State
```cpp
int ncols_, nlines_;           // Terminal dimensions
int cursor_col_, cursor_line_; // Cursor position in view
std::string status_;           // Status message
std::string filename_;         // Current filename
bool cmd_mode_;                // Command mode active
bool quit_flag_;               // Exit requested
bool insert_mode_;             // Insert vs overwrite
bool quote_next_;              // ^P literal insert mode
bool filter_mode_;             // External filter entry mode
bool area_selection_mode_;     // Rectangular selection active
```

### Current Line Buffer
```cpp
std::string current_line_;        // Editing buffer
int current_line_no_;             // Which line is buffered (-1 = invalid)
bool current_line_modified_;      // Buffer needs writing back
```

### Segment Chain (large file model)
```cpp
struct Segment {
    unsigned line_count;                   // Number of lines in segment
    int file_descriptor;                   // File containing data, or -1 for empty lines
    long file_offset;                      // Offset in file where data begins
    std::vector<unsigned short> line_lengths; // Byte length of each line (including \n)
};
```

### Enhanced Clipboard
```cpp
class Clipboard {
    std::vector<std::string> lines_;  // Content lines
    Parameters params_;              // Bounds and type tracking

    void serialize(std::ostream&);   // Save/restore across sessions
    void deserialize(std::istream&);
};
```

### Workspaces
```cpp
class Workspace {
    Segment *head_;         // Segment chain head
    Segment *cursegm_;      // Current segment pointer
    int topline_;           // Viewport top line
    int basecol_;           // Horizontal scroll offset
    bool modified_;         // Change tracking
    Tempfile &tempfile_;    // Shared temp file
};

// Editor maintains two:
std::unique_ptr<Workspace> wksp_;      // Primary workspace
std::unique_ptr<Workspace> alt_wksp_;  // Alternative workspace
```

### Macro System
```cpp
std::map<char, Macro> macros_;  // char -> macro data
class Macro {
    enum Type { POSITION, BUFFER } type_;
    std::pair<int,int> position_;         // line,col for POSITION
    std::vector<std::string> buffer_;     // lines for BUFFER
    Parameters bounds_;                  // rectangular extents
};
```

#### Macro Operations:
- **Position markers**: `>x` (set), `$x` (goto) - remember cursor positions
- **Named buffers**: `^C>x` (set buffer), `^V$x` (paste buffer) - store/copy rectangular blocks

## Build System

### CMake Structure
```cmake
v_edit_lib (static)
    ↳ core.cpp display.cpp edit_mode.cpp cmd_mode.cpp file.cpp files.cpp
      clipboard.cpp session.cpp filter.cpp segment.cpp signal.cpp macro.cpp

ve (executable) ← v_edit_lib

v_edit_tests ← v_edit_lib + GoogleTest + TmuxDriver
```

- **Library**: `v_edit_lib` includes all editor logic except `main.cpp`
- **Executable**: `ve` links only main.cpp + library
- **Warnings**: `-Wall -Werror -Wshadow` (varies by compiler)
- **Install**: Both binary and library installed
- **Tests**: GoogleTest 1.15.2 with TmuxDriver for ncurses UI testing

### Build Commands
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build
make -C build install  # Optional
```

### Testing Framework
- **Unit tests**: Direct Editor/Workspace instantiation
- **Integration tests**: tmux-based UI testing using TmuxDriver
- **Test isolation**: Unique tmux socket per PID
- **Framework**: GoogleTest 1.15.2 with TmuxDriver for ncurses UI testing

### Running Tests
```bash
cd build
make v_edit_tests
./tests/v_edit_tests
```

## Key Implementation Patterns

### Main Loop (core.cpp)
```cpp
timeout(200);  // 200ms non-blocking input
for (;;) {
    check_interrupt();
    move(cursor_line_, cursor_col_);
    int ch = journal_read_key();
    if (ch == ERR) {
        draw();  // Redraw even without input
    } else {
        journal_write_key(ch);
        handle_key(ch);
        draw();
    }
    if (quit_flag_) break;
}
```

### Current Line Editing Pattern
```cpp
// To edit line N:
get_line(N);              // Load into current_line_
current_line_ = "...";    // Edit buffer contents
current_line_modified_ = true;
put_line();               // Write back to segment chain
```

### Key Handling Flow
```
handle_key()
  ├─ cmd_mode_? → handle_key_cmd() ┌─ area_selection_mode_?
  │                                ├─ rectangular operations (^C/^Y/^O)
  │                                └─ parameter/movement processing
  └─ handle_key_edit() ──────────────┼─ F-keys, ^X prefix, movement
                                     └─ character editing into current_line_
```

### Session State
- **State file**: `~/.ve/session`
- **Journal files**: `/tmp/rej{tty}{user}` for replay
- **Modes**: `0`=normal, `1`=restore, `2`=replay
- **Restart logic**: Arg parsing determines startup mode

### Signal Handling
- **SIGINT**: Sets `interrupt_flag_` (user cancel)
- **Fatal signals**: Emergency state save + graceful exit
- **Singleton pattern**: Static `instance_` for handlers

## Development Conventions

### Code Style
- **Indentation**: 4 spaces, no tabs (C++ and CMake)
- **Formatting**: `make reindent` for clang-format
- **Naming**:
  - Private members: underscore suffix (`ncols_`, `wksp_`)
  - Public API: no suffix (`get_lines()`, `topline()`)
  - Methods: snake_case (`handle_key()`, `draw()`)
  - Fields and local variables: snake_case (`start_line`, `first_seg`)
  - Classes/Structs: PascalCase (`Editor`, `Clipboard`, `Workspace`)

### Development Workflow (TDD)
- **When Adding Features**: Write test first → Implement minimum code → Refactor
- **When Fixing Bugs**: Write test that reproduces bug → Fix → Verify → Refactor
- **Always run tests** after each change to ensure they pass
- **Never write implementation code** without corresponding tests

### Architecture Decisions
- **Dual workspaces**: Independent editing contexts with shared tempfile
- **Segment chains**: Efficient large file handling with lazy loading
- **Current line buffer**: Fast in-memory editing with selective persistence
- **Parameter system**: Enhanced command parsing with counts/bounds
- **Serialization**: Persistent clipboard and macro state

### File Organization
- **Headers**: Classes, structures, constants in `.h` files
- **Implementation**: Single-responsibility `.cpp` files
- **Test files**: Comprehensive unit and integration tests
- **Documentation**: Usage docs separate from AI context

### Debugging
- Create focused unit tests to reproduce the issue
- Insert debug prints for visibility
- Use `lldb` for debugging crashes

### Code Navigation
- **compile_commands.json**: Generated in `build/` directory after CMake configuration. Used by LSP servers (clangd) for accurate code analysis and IDE navigation.

## Repository Structure
```
/Users/vak/Project/Editors/v-edit/
├── cmake/                      # Build configuration
├── docs/                       # Documentation
│   ├── AI_Prompt.md            # This file
│   ├── Commands.md             # User command reference
│   ├── Rectangular_Blocks.md   # Block operations
│   └── Testing.md              # Testing guidelines
├── prototype/                  # Original C prototype
├── tests/                      # Comprehensive test suite
│   ├── CMakeLists.txt          # Test build config
│   ├── TmuxDriver.*            # UI testing framework
│   └── *_test.cpp              # Individual test files
├── CMakeLists.txt              # Root build file
├── Makefile                    # Convenience targets
├── *.cpp *.h                   # Source code and headers
└── README.md                   # User documentation
```

## Known Issues & Future Work
- **Performance**: Segment chain operations may need optimization for very large files
- **Memory usage**: Dual data model could be optimized further
- **Feature**: Macro system could be expanded
- **Testing**: More integration tests for complex editing scenarios

## Startup Behavior
- **No args**: Attempt session restore from `~/.ve/session`
- **`-` arg**: Replay keystrokes from journal file
- **File arg**: Open specified file for editing
- **Help args**: Display usage information

This comprehensive AI context ensures consistent and accurate assistance across all development tasks.
