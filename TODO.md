# Fix Column Indexing Bugs in Editor::handle_key_edit()

## Problem Summary
The basic editing operations in `handle_key_edit()` use `cursor_col_` directly to index into `current_line_`, but `cursor_col_` is a screen-relative position (0 to ncols_-1). When horizontal scrolling is active (`wksp_->view.basecol > 0`), this causes edits to occur at the wrong position in the line.

## Root Cause
- `cursor_col_` = screen-relative cursor position
- `wksp_->view.basecol` = horizontal scroll offset
- Actual column in line = `basecol + cursor_col_`

## Implementation Plan

### Phase 1: Add Helper Method
- [x] Add `get_actual_col()` method to editor.h (returns size_t)
- [x] Implement method in edit.cpp to return `wksp_->view.basecol + cursor_col_`

### Phase 2: Fix Each Editing Operation in input.cpp

#### BACKSPACE Operation (around line 803)
- [x] Calculate `actual_col` at start of operation
- [x] Replace `cursor_col_ > 0` check with `actual_col > 0`
- [x] Replace `cursor_col_ <= (int)current_line_.size()` with `actual_col <= current_line_.size()`
- [x] Replace `cursor_col_ - 1` in erase with `actual_col - 1`
- [x] Keep line joining logic (already correct)

#### DELETE Operation (around line 824)
- [x] Calculate `actual_col` at start of operation
- [x] Replace `cursor_col_ < (int)current_line_.size()` with `actual_col < current_line_.size()`
- [x] Replace `cursor_col_` in erase with `actual_col`
- [x] Keep line joining logic (already correct)

#### ENTER/Newline Operation (around line 838)
- [x] Calculate `actual_col` at start of operation
- [x] Replace `cursor_col_ < (int)current_line_.size()` with `actual_col < current_line_.size()`
- [x] Replace `cursor_col_` in substr/erase with `actual_col`

#### TAB Operation (around line 862)
- [x] Calculate `actual_col` at start of operation
- [x] Replace `cursor_col_` in insert with `actual_col`

#### Character Insertion/Overwrite (around line 869)
- [x] Calculate `actual_col` at start of operation
- [x] Replace `cursor_col_` in insert with `actual_col`
- [x] Replace `cursor_col_ < (int)current_line_.size()` with `actual_col < current_line_.size()`
- [x] Replace `current_line_[cursor_col_]` with `current_line_[actual_col]`

### Phase 3: Testing
- [x] Build verification - Project compiles successfully
- [x] Test editing without horizontal scroll (basecol = 0) - **22/22 tests PASSED** ✅
  - [x] BACKSPACE operation (3 tests)
  - [x] DELETE operation (3 tests)
  - [x] ENTER/newline operation (3 tests)
  - [x] TAB operation (3 tests)
  - [x] Character insertion (3 tests)
  - [x] Character overwrite (3 tests)
  - [x] Edge cases and complex scenarios (4 tests)
- [x] Test editing with horizontal scroll (basecol > 0) - **28/28 tests PASSED** ✅
  - [x] BACKSPACE with scroll (2 tests)
  - [x] DELETE with scroll (2 tests)
  - [x] ENTER/newline with scroll (2 tests)
  - [x] TAB with scroll (2 tests)
  - [x] Character insertion with scroll (2 tests)
  - [x] Character overwrite with scroll (2 tests)
  - [x] Edge cases and complex scenarios (4 tests)
  - [x] Bug fix verification test (1 test)
  - [x] Cursor beyond line end scenarios (5 tests)
  - [x] Line joining operations with scroll (7 tests)

## Status: All Testing Complete ✅

All code changes have been implemented and **thoroughly tested**:
- ✅ Implementation complete - all 5 editing operations fixed
- ✅ Project builds successfully
- ✅ Comprehensive unit tests created (**50 tests total**)
  - 22 tests for basecol = 0 (no horizontal scroll)
  - 28 tests for basecol > 0 (with horizontal scroll)
- ✅ **All 50 tests passing (100% pass rate)**

**Test Coverage:**
- ✅ BACKSPACE operation with and without scroll
- ✅ DELETE operation with and without scroll
- ✅ ENTER/newline operation with and without scroll
- ✅ TAB operation with and without scroll
- ✅ Character insertion with and without scroll
- ✅ Character overwrite with and without scroll
- ✅ Edge cases (empty lines, long lines, multiple operations)
- ✅ Bug fix verification (specific scenario that was broken)
- ✅ Cursor beyond line end scenarios (with and without scroll)
- ✅ Line joining operations (backspace and delete at line boundaries with scroll)

**Files Created:**
- `tests/basic_editing_test.cpp` (22 tests)
- `tests/horizontal_scroll_editing_test.cpp` (28 tests)

**Recommendation:**
Runtime testing with actual editor to verify fixes work correctly in real-world usage scenarios.

## Notes
- `cursor_col_` remains unchanged (screen position)
- Only the indexing into `current_line_` uses `actual_col`
- After edits, `ensure_cursor_visible()` handles scroll adjustment
