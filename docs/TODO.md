# TODO - Segment-Based Editing Implementation

## Overview
This document tracks the implementation of segment-based editing, migrating from a temporary line cache to a proper segment-based architecture.

## Current Status

### Completed ✅

1. **Removed temporary line cache**
   - Removed `line_cache` from `editor.h`
   - Removed all `line_cache` lookups from `file.cpp`

2. **Modified `build_segment_chain_from_lines()`**
   - Now writes lines to temp file instead of using `fdesc=0`
   - No more "in-memory" segments
   - All segments are either from original file, temp file, or contain empty lines (`fdesc == -1`)

3. **Added assertions for `fdesc`**
   - `assert(seg->fdesc != 0)` in critical paths
   - Ensures segments always have valid file descriptors
   - Empty lines use `fdesc == -1`

4. **Fixed `insert_segments()`**
   - Now updates `nlines_` correctly
   - Fixed segment linking logic to insert BEFORE the target segment
   - Properly handles insertion at beginning, middle, and end

5. **Fixed `read_line_from_segment()`**
   - Returns `""` for empty lines (length 1, just newline)
   - Properly handles segments from original file, temp file, and empty lines

6. **Fixed `delete_segments()`**
   - Correctly deletes lines from `from` to `to` inclusive
   - Updates `nlines_` correctly
   - Properly handles segment breaking and unlinking
   - All delete-related tests now passing

7. **Added unit tests**
   - `BuildSegmentChainFromLines` - tests writing to temp file
   - `InsertSegmentsUpdatesNlines` - tests insert functionality
   - `InsertSegmentsAtStart` - tests insertion at beginning
   - `DeleteSegmentsUpdatesNlines` - tests delete functionality  
   - `DeleteSegmentsMiddle` - tests deleting from middle
   - `InsertAndDeleteWorkflow` - tests complete workflow

### Pending ❌

1. **Fix remaining TmuxDriver test failures**
   - `TmuxDriver.ExternalFilterSortsLines`
   - `TmuxDriver.SaveAndExitWritesFile`
   - `TmuxDriver.VerticalScrollingShowsLaterLines`
   - `TmuxDriver.SegmentsHandleEmptyFile`
   - `TmuxDriver.SegmentsPreserveFileContentOnSave`

2. **Code cleanup**
   - Remove TODO comments
   - Clean up debug output
   - Review and optimize segment operations

## Test Results

**Last run**: 77 tests
- **Passing**: 72 tests ✅
- **Failing**: 5 tests ❌

## Key Design Decisions

1. **No in-memory segments**: Every segment references a file (original or temp)
2. **Empty lines use `fdesc == -1`**: Special case for segments containing only newlines
3. **Assert `fdesc != 0`**: Ensure segments always have valid file descriptors
4. **Temp file shared across segments**: All modified/new lines written to same temp file
5. **Line length storage**: Uses `sizes` vector instead of encoded data array

## Implementation Notes

### Segment Structure
```cpp
class Segment {
    int fdesc;  // -1 for empty lines, >0 for file fd
    std::vector<unsigned short> sizes;  // Line lengths including newline
    int nlines;
    long seek;
};
```

### File Descriptor Rules
- `fdesc > 0`: Segment from file (original_fd_ or tempfile_fd_)
- `fdesc == -1`: Empty lines (only newlines)
- `fdesc == 0`: Tail marker (end of chain)
- **Never** use `fdesc == 0` for content segments

### Temp File Management
- Opened once per workspace via `open_temp_file()`
- Kept open for the lifetime of the workspace
- Modified lines written sequentially with `write_line_to_temp()`
- `tempseek_` tracks current position in temp file

## Next Steps

1. Investigate and fix remaining TmuxDriver test failures (5 tests)
2. Add more unit tests for edge cases
3. Performance optimization if needed
4. Documentation updates

## References

- Original plan: Complete `put_line()` implementation, integrate clipboard/macro operations
- Segment-based editing model: Prototype implementation in `prototype/r.edit.c`
- Related files: `workspace.cpp`, `file.cpp`, `segment.cpp`, `segment.h`
