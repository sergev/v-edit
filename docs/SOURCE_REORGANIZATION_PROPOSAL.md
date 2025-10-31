# C++ Source File Reorganization Proposal

## Current Issues

1. **Functional Overlap**: `edit.cpp` contains low-level editing operations and movement/search, while `edit_mode.cpp` handles key bindings for edit mode. This is confusing.

2. **Naming Mismatch**: `files.cpp` doesn't handle multiple files; it's about workspace switching and help file operations.

3. **Clarification Needed**: `copy_paste.cpp` contains both clipboard operations and macro (named buffer) operations, which are conceptually related but could be clearer.

4. **Mixed Concerns**: `file.cpp` mixes current line buffer operations with file I/O operations.

## Recommended Reorganization

### Option 1: Minimal Changes (Conservative)

#### Merge Operations
- **Keep `clipboard.cpp/h`** - Already well-organized

#### Rename for Clarity
- **`files.cpp` → `workspace_help.cpp`**: Reflects actual functionality (alternative workspace and help file)
- **`copy_paste.cpp` → `clipboard_macro.cpp`**: Clarifies it handles both clipboard and macro (named buffer) operations

#### Clear Separation
- **Keep `edit.cpp`**: Low-level editing operations (edit_*, move_*, search_*, line operations)
- **Keep `edit_mode.cpp`**: Edit mode key bindings (handle_key_edit)
- **Keep `cmd_mode.cpp`**: Command mode key bindings (handle_key_cmd)
- **Keep `file.cpp`**: File I/O operations (load, save, get_line, put_line)

This option maintains the current structure while fixing naming issues.

### Option 2: Functional Grouping (Recommended)

#### Core Display & Control
- **`core.cpp`** - Main loop, startup, initialization (KEEP as-is)
- **`display.cpp`** - Screen drawing, colors, status (KEEP as-is)

#### Input Handling (User Input → Operations)
- **`key_bindings.cpp`** (NEW: merge `edit_mode.cpp` + `cmd_mode.cpp`)
  - `handle_key_edit()`
  - `handle_key_cmd()`
  - All key handling logic in one place

#### File I/O & Buffer Management
- **`file_io.cpp`** (RENAME from `file.cpp`, expand)
  - `load_file_segments()`, `save_file()`, `save_as()`
  - `open_initial()`
  - File backup operations
  
- **`line_buffer.cpp`** (NEW: extract from `file.cpp`)
  - `get_line()`, `put_line()`
  - Current line buffer management
  - Line buffer state tracking

#### Text Operations
- **`text_operations.cpp`** (RENAME from `edit.cpp`)
  - Low-level editing: `edit_backspace()`, `edit_delete()`, `edit_enter()`, `edit_tab()`, `edit_insert_char()`
  - Movement: `move_left()`, `move_right()`, `move_up()`, `move_down()`
  - Search: `search_forward()`, `search_backward()`, `search_next()`, `search_prev()`
  - Line operations: `insertlines()`, `deletelines()`, `splitline()`, `combineline()`

#### Clipboard & Buffers
- **`clipboard_operations.cpp`** (RENAME from `copy_paste.cpp`)
  - `picklines()`, `paste()`, `pickspaces()`, `closespaces()`, `openspaces()`
  
- **`macros.cpp`** (EXTRACT from `copy_paste.cpp`)
  - `save_macro_position()`, `goto_macro_position()`
  - `save_macro_buffer()`, `paste_macro_buffer()`
  - `mdeftag()`

- **`clipboard.h/cpp`** (KEEP - data structure only)

#### External Operations
- **`filter.cpp`** - External filter execution (KEEP as-is)

#### Workspace Management
- **`workspace.cpp/h`** - Core workspace class (KEEP as-is)
- **`workspace_management.cpp`** (RENAME from `files.cpp`)
  - Alternative workspace switching
  - Help file operations

#### Session & State
- **`session.cpp`** (KEEP as-is)
- **`signal.cpp`** (KEEP as-is)
- **`segment.cpp/h`** (KEEP as-is)
- **`tempfile.cpp/h`** (KEEP as-is)
- **`macro.h`** (KEEP as-is)

#### Command Processing
- **`command_processing.cpp`** (NEW: extract parameter handling logic)
  - Command parameter parsing
  - Parameter validation
  - Currently scattered across `cmd_mode.cpp`

### Option 3: Layer-Based Architecture (Ambitious)

#### Presentation Layer
- `display.cpp`, `ui.cpp` (extract status/messages)

#### Input Layer
- `key_bindings.cpp`, `key_mapping.h`

#### Business Logic Layer
- `editor_operations.cpp` (all editing operations)
- `navigation.cpp` (movement, search, goto)
- `clipboard_manager.cpp`, `buffer_manager.cpp`

#### Data Layer
- `workspace.cpp`, `segment.cpp`, `tempfile.cpp`, `file_io.cpp`, `line_buffer.cpp`

#### Infrastructure
- `session.cpp`, `signal.cpp`, `filter.cpp`, `journal.cpp` (extract from core.cpp)

## Recommendations by Priority

### High Priority (Quick Wins)
1. **Rename `files.cpp` → `workspace_help.cpp`** (5 minutes, high clarity gain)
2. **Rename `copy_paste.cpp` → `clipboard_macro.cpp`** (2 minutes)
3. **Consider renaming `edit.cpp` → `text_operations.cpp`** (2 minutes)

### Medium Priority (Structural Improvements)
1. **Extract `line_buffer.cpp` from `file.cpp`** (30 minutes)
2. **Extract macros from `copy_paste.cpp` → `macros.cpp`** (45 minutes)
3. **Consider merging `edit_mode.cpp` + `cmd_mode.cpp` → `key_bindings.cpp`** (1 hour)

### Low Priority (Large Refactoring)
1. Layer-based architecture (only if doing major architectural changes)

## Impact Assessment

### Minimal Change Option (Option 1)
- **Build impact**: 2 file renames, no functional changes
- **Risk**: Low
- **Benefit**: Much clearer naming

### Functional Grouping Option (Option 2)
- **Build impact**: ~5-7 file renames/merges/splits, some method moves
- **Risk**: Medium (need thorough testing)
- **Benefit**: Better organization, clearer responsibilities

### Layer-Based Option (Option 3)
- **Build impact**: Major reorganization, 10+ file changes
- **Risk**: High (significant testing needed)
- **Benefit**: Ideal architecture for future growth

## My Recommendation

**Start with Option 1** (Minimal Changes) for immediate clarity, then consider **Option 2** (Functional Grouping) for the next phase if you want better long-term organization. Option 3 is overkill for current needs but worth considering if you're planning major feature additions.

**NOTE**: After reviewing the codebase in detail, I recommend starting with **Option 1** (renaming for clarity), then doing Option 2 incrementally if needed. The current organization is actually quite good, and Option 3 would require extensive testing.

## Implementation Checklist (Option 2)

- [ ] Rename `files.cpp` → `workspace_management.cpp`
- [ ] Rename `edit.cpp` → `text_operations.cpp`
- [ ] Rename `file.cpp` → `file_io.cpp`
- [ ] Create `line_buffer.cpp` with get_line/put_line
- [ ] Create `macros.cpp` extracting macro operations from `copy_paste.cpp`
- [ ] Rename `copy_paste.cpp` → `clipboard_operations.cpp`
- [ ] Update all includes across codebase
- [ ] Update `CMakeLists.txt`
- [ ] Run full test suite
- [ ] Update documentation

## File Size Guidance

After reorganization, target sizes:
- Core files: 200-400 lines (key_bindings.cpp might be 500+)
- Operation files: 150-300 lines
- Data structure files: 100-200 lines
- If a file exceeds 600 lines, consider further splitting

