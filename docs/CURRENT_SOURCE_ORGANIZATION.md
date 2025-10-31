# Current C++ Source File Organization

## Overview

The `ve` editor has **19 C++ source files** (+ headers) organized into roughly functional areas. Here's the current state:

## Core Architecture Files

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `editor.h` | 196 | Main Editor class definition | ⚠️ Large, but appropriate for architecture |
| `core.cpp` | ~165 | Main loop, startup, model initialization | ✅ Well-focused |
| `display.cpp` | ~174 | Screen drawing, colors, status display | ✅ Clear separation |

## Input Handling

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `edit_mode.cpp` | ~284 | Edit mode key bindings (handle_key_edit) | ⚠️ Could merge with cmd_mode |
| `cmd_mode.cpp` | ~500 | Command mode key bindings (handle_key_cmd) | ⚠️ Large, mixes binding + logic |

## Text & Editing Operations

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `edit.cpp` | ~546 | Low-level editing, movement, search, line ops | ⚠️ Does too much, unclear name |
| `copy_paste.cpp` | ~285 | Clipboard & macro operations | ⚠️ Mixes clipboard + macros |

## File & Buffer Management

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `file.cpp` | ~134 | File I/O + current line buffer | ⚠️ Two unrelated concerns |
| `files.cpp` | ~133 | Alternative workspace + help file | ❌ Misleading name! |
| `workspace.cpp` | ~877 | Workspace class implementation | ✅ Well-focused, large is OK |

## Data Structures

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `segment.cpp/h` | ~166 | Segment chain for large files | ✅ Clear |
| `tempfile.cpp/h` | ~130 | Temporary file management | ✅ Clear |
| `clipboard.h/cpp` | ~189 | Clipboard data structure | ✅ Clear |
| `macro.cpp/h` | ~143 | Macro/buffer data structure | ✅ Clear |

## Infrastructure

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `session.cpp` | ~110 | Session save/restore, journaling | ✅ Well-focused |
| `signal.cpp` | ~86 | Signal handling | ✅ Well-focused |
| `filter.cpp` | ~117 | External filter execution | ✅ Well-focused |

## Supporting Files

| File | Purpose |
|------|---------|
| `parameters.h` | Command parameter definitions |
| `main.cpp` | Entry point |

## Issues Identified

### 🔴 High Priority Issues

1. **`files.cpp`**: Name is misleading - doesn't handle multiple files, handles workspace switching and help
2. **`edit.cpp`**: Unclear name - actually contains text operations, not just "editing"

### ⚠️ Medium Priority Issues

3. **`file.cpp`**: Two concerns - file I/O vs. current line buffer management
4. **`copy_paste.cpp`**: Two concerns - clipboard operations vs. macro (named buffer) operations
5. **`cmd_mode.cpp`**: Too large (~500 lines), mixes key binding with business logic
6. **`edit.cpp`**: Does too much (546 lines) - editing, movement, search, line ops

### 💡 Design Opportunities

7. **`edit_mode.cpp` + `cmd_mode.cpp`**: Could be unified as key binding layer
8. **Macro operations**: Currently in `copy_paste.cpp` but conceptually separate

## Current Organization Philosophy

The codebase follows a **function-based** organization:
- One file per major feature area
- Minimal class splitting (Editor is monolithic)
- Pragmatic grouping by rough functionality

This works but could be improved for:
- Better discoverability
- Clearer responsibilities
- Easier testing
- Future growth

## File Size Distribution

- Small (< 200 lines): 12 files
- Medium (200-400 lines): 5 files
- Large (400-600 lines): 2 files (`cmd_mode.cpp`, `edit.cpp`)
- Very Large (> 800 lines): 1 file (`workspace.cpp`)

Generally healthy, but a few files are doing too much.

## Dependencies

```
editor.h
├── clipboard.h → clipboard.cpp
├── macro.h → macro.cpp
├── parameters.h (header only)
├── segment.h → segment.cpp
├── tempfile.h → tempfile.cpp
└── workspace.h → workspace.cpp

Main flow:
main.cpp
└── core.cpp (main loop)
    ├── display.cpp
    ├── edit_mode.cpp
    ├── cmd_mode.cpp
    ├── edit.cpp
    ├── copy_paste.cpp
    ├── file.cpp
    ├── files.cpp
    ├── filter.cpp
    ├── session.cpp
    └── signal.cpp
```

## Recommendation

See `SOURCE_REORGANIZATION_PROPOSAL.md` for detailed recommendations.

