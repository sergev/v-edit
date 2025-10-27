# ve — Minimal ncurses-based text editor

A lightweight terminal-based text editor supporting multiple files, rectangular block operations, macros, and session management.

## Build & Install

```bash
make
make install          # Optional
```

## Usage

```bash
ve                    # Restore last session
ve -                  # Replay keystrokes from journal
ve [file]             # Open file
ve --help             # Show help
ve --version          # Show version
```

## Core Features

### Editing Modes
- **Insert mode** (default): Characters insert at cursor
- **Overwrite mode**: Characters replace existing text (`^X i` to toggle)
- **Command mode**: `F1` or `^A` for commands

### Basic Editing
- **Navigation**: Arrow keys, Home/End, Page Up/Down
- **Text input**: Printable characters, Tab (4 spaces), Enter (split line)
- **Character operations**: Backspace, Delete (`^D`), Quote next (`^P`)
- **Line operations**: Copy (`^C`/F5), Paste (`^V`/F6), Delete (`^Y`), Insert blank (`^O`)

### Advanced Operations
- **Search**: Forward (`/text` or `^F`), backward (`?text` or `^B`)
- **Navigation**: Goto line (`g<number>` or `:<number>`), next/prev match (`n`/`N`)
- **Rectangular blocks**: Select with cursor, copy (`^C`), delete (`^Y`), insert spaces (`^O`)
- **Macros**: Position markers (`>x`, `$x`) and text buffers
- **External filters**: `F4` to run shell commands on selected lines
- **Alternative workspace**: `^N` for two independent editing contexts

### File Management
- **Multiple files**: `F3` to switch, `o<filename>` to open
- **Session persistence**: Auto-save state and keystroke journals
- **Save operations**: `F2`, `^A s`, `:w`, `^X ^C` (quit)

## Naming Conventions

- Private class member variables use underscore suffix (e.g., `ncols_`, `cursor_line_`)
- Public methods and accessors use no suffix (e.g., `get_lines()`, `topline()`)

## Documentation

- **[Commands Reference](docs/Commands.md)** — Complete command documentation
- **[Rectangular Blocks](docs/Rectangular_Blocks.md)** — Block operations guide
- **[Testing](docs/Testing.md)** — Build and test information
- **[AI Prompt](docs/AI_Prompt.md)** — Technical context for AI agents developing this codebase
