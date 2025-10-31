# ve — Minimal ncurses-based text editor

A lightweight terminal-based text editor supporting multiple files, rectangular block operations, macros, dual workspaces, and session management.

## Build & Install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build
make -C build install  # Optional
```

## Development

**⚠️ MANDATORY: All development in this project MUST follow Test-Driven Development (TDD). Write tests first, then implement.**

See [Testing.md](docs/Testing.md) for test execution details.

## Usage

```bash
ve                    # Restore last session or create new empty file
ve [file]             # Open file
ve -                  # Replay keystrokes from journal
ve --help             # Show help
ve --version          # Show version
```

## Core Features

### Editing Modes
- **Insert mode** (default): Characters insert at cursor
- **Overwrite mode**: Characters replace existing text (`^X i` to toggle, disabled by default)
- **Command mode**: `F1` or `^A` to enter commands
- **Area selection mode**: Define rectangular regions by cursor movement in command mode

### Basic Editing
- **Navigation**: Arrow keys, Home/End, Page Up/Down
- **Text input**: Printable characters, Tab (4 spaces), Enter (split line)
- **Character operations**: Backspace, Delete (`^D`), Quote next (`^P`)
- **Line operations**: Copy (`^C`/F5), Paste (`^V`/F6), Delete (`^Y`), Insert blank (`^O`)

### Advanced Operations
- **Search**: Forward (`/text` or `^F`), backward (`?text` or `^B`), next/prev match (`n`/`N`)
- **Navigation**: Goto line (`g<number>` or `:<number>` or just `<number>`)
- **Rectangular blocks**: Select area with cursor in command mode, copy (`^C`), delete (`^Y`), insert spaces (`^O`)
- **Macros**: Position markers (`>x`, `$x`) and named text buffers (`^C>name`, `^V$name`)
- **External filters**: `F4` to run shell commands on selected lines
- **Dual workspaces**: `^N` to switch to alternative workspace (help), `F3` to switch between workspaces
- **Area selection**: Move cursor in command mode to define rectangular regions for operations

### File Management
- **Multiple files**: `F3` to switch workspaces, `o<filename>` to open file in current workspace
- **Session persistence**: Auto-save state (`~/.ve/session`) and keystroke journals (`/tmp/rej{tty}{user}`)
- **Journal replay**: Use `ve -` to replay previous session keystrokes
- **Save operations**: `F2`, `^A s`, `:w`, `^X ^C` (save and quit)

## Source Organization

The codebase follows a layer-based architecture:
- **Presentation Layer**: UI rendering and main loop (`core.cpp`, `display.cpp`)
- **Input Layer**: Keyboard input handling (`key_bindings.cpp`)
- **Business Logic Layer**: Editing operations (`ops.cpp`), clipboard (`clipboard.cpp`), macros (`buffer.cpp`)
- **Data Layer**: File I/O (`file.cpp`), workspace management (`workspace.cpp`), segments (`segment.cpp`), temp files (`tempfile.cpp`)
- **Infrastructure**: Session management and signals (`session.cpp`), help and filters (`help.cpp`)

## Naming Conventions

- Private class member variables use underscore suffix (e.g., `ncols_`, `cursor_line_`)
- Public methods and accessors use no suffix (e.g., `get_lines()`, `topline()`)

## Documentation

- **[Commands Reference](docs/Commands.md)** — Complete command documentation
- **[Rectangular Blocks](docs/Rectangular_Blocks.md)** — Block operations guide
- **[Testing](docs/Testing.md)** — Build and test information
- **[AI Prompt](docs/AI_Prompt.md)** — Technical context for AI agents developing this codebase
