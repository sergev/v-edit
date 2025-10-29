# Refactor handle_key_edit() for Better Testability

## Problem Statement

Current tests manipulate the `current_line_` buffer directly to simulate editing operations, but don't test the actual `handle_key_edit()` method. This means:
- Key handling logic is not tested
- Real code paths are not validated
- Bugs in the integration between key handling and editing could be missed

## Solution: Extract Backend Editing Methods

Separate UI/key routing from core editing logic by extracting testable backend methods.

---

## Phase 1: Extract Backend Editing Methods

### Goal
Create private editing methods that contain the core logic, making them independently testable.

### New Methods to Add to editor.h

```cpp
#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_H_
private:
#endif
    // Backend editing operations (testable)
    void edit_backspace();           // Handle backspace operation
    void edit_delete();              // Handle delete operation
    void edit_enter();               // Handle enter/newline operation
    void edit_tab();                 // Handle tab insertion
    void edit_insert_char(char ch);  // Handle character insertion/overwrite
```

### Implementation Details

#### 1. `edit_backspace()` (extract from lines 803-824 in input.cpp)
- Calculate `actual_col` using `get_actual_col()`
- If `actual_col > 0` and within line bounds:
  - Erase character at `actual_col - 1`
  - Decrement `cursor_col_`
  - Mark line as modified
- Else if at start of line (`actual_col == 0`) and not first line:
  - Join with previous line
  - Update cursor position
  - Delete current line from workspace
- Call `put_line()` and `ensure_cursor_visible()`

#### 2. `edit_delete()` (extract from lines 826-841)
- Calculate `actual_col` using `get_actual_col()`
- If `actual_col < current_line_.size()`:
  - Erase character at `actual_col`
  - Mark line as modified
- Else if at end of line and next line exists:
  - Join with next line
  - Delete next line from workspace
- Call `put_line()` and `ensure_cursor_visible()`

#### 3. `edit_enter()` (extract from lines 843-862)
- Calculate `actual_col` using `get_actual_col()`
- If `actual_col < current_line_.size()`:
  - Extract tail from `actual_col` to end
  - Truncate current line at `actual_col`
- Mark line as modified
- Call `put_line()`
- Insert new line with tail content
- Update cursor position (move to next line, column 0)
- Call `ensure_cursor_visible()`

#### 4. `edit_tab()` (extract from lines 864-870)
- Calculate `actual_col` using `get_actual_col()`
- Insert 4 spaces at `actual_col`
- Increment `cursor_col_` by 4
- Mark line as modified
- Call `put_line()` and `ensure_cursor_visible()`

#### 5. `edit_insert_char(char ch)` (extract from lines 872-895)
- Calculate `actual_col` using `get_actual_col()`
- Handle quote mode if `quote_next_` is true
- If `insert_mode_`:
  - Insert character at `actual_col`
- Else (overwrite mode):
  - If within line bounds: replace character
  - Else: append character
- Increment `cursor_col_`
- Mark line as modified
- Call `put_line()` and `ensure_cursor_visible()`
- Clear `quote_next_` flag if it was set

### Tasks for Phase 1

- [x] Add method declarations to editor.h (in testable section)
- [x] Implement `edit_backspace()` in edit.cpp
- [x] Implement `edit_delete()` in edit.cpp
- [x] Implement `edit_enter()` in edit.cpp
- [x] Implement `edit_tab()` in edit.cpp
- [x] Implement `edit_insert_char(char ch)` in edit.cpp
- [x] Ensure all methods properly handle edge cases
- [x] Methods use `get_line(curLine)` and `get_actual_col()` internally
- [x] All 50 tests pass - no regressions

---

## Phase 2: Refactor handle_key_edit()

### Goal
Replace inline editing code with calls to the extracted methods.

### Changes to input.cpp

#### Before (current code):
```cpp
if (ch == KEY_BACKSPACE || ch == 127) {
    size_t actual_col = get_actual_col();
    if (actual_col > 0) {
        if (actual_col <= current_line_.size()) {
            current_line_.erase(actual_col - 1, 1);
            current_line_modified_ = true;
            cursor_col_--;
        }
    } else if (curLine > 0) {
        // ... line joining logic ...
    }
    put_line();
    ensure_cursor_visible();
    return;
}
```

#### After (refactored):
```cpp
if (ch == KEY_BACKSPACE || ch == 127) {
    edit_backspace();
    return;
}
```

### Similar Changes for Other Operations

- `KEY_DC` → `edit_delete()`
- `'\n' || KEY_ENTER` → `edit_enter()`
- `'\t'` → `edit_tab()`
- `ch >= 32 && ch < 127` → `edit_insert_char((char)ch)`

### Tasks for Phase 2

- [x] Replace BACKSPACE handling with `edit_backspace()` call
- [x] Replace DELETE handling with `edit_delete()` call
- [x] Replace ENTER handling with `edit_enter()` call
- [x] Replace TAB handling with `edit_tab()` call
- [x] Replace character insertion with `edit_insert_char()` call
- [x] Remove now-redundant `int curLine` and `get_line(curLine)` calls (done - moved to backend methods)
- [x] Backend methods handle `current_line_` loading internally
- [x] Verify all edge cases still work correctly
- [x] Build and test existing functionality - ALL 50 TESTS PASS

---

## Phase 3: Update Unit Tests

### Goal
Modify existing tests to call the actual backend editing methods instead of manipulating buffers directly.

### Changes to tests/basic_editing_test.cpp

#### Before (current test):
```cpp
TEST_F(BasicEditingTest, BackspaceMiddleOfLine) {
    CreateLine(0, "Hello World");
    LoadLine(0);
    editor->cursor_col_ = 6;
    
    // Manually manipulate buffer
    size_t actual_col = GetActualCol();
    if (actual_col > 0 && actual_col <= editor->current_line_.size()) {
        editor->current_line_.erase(actual_col - 1, 1);
        editor->current_line_modified_ = true;
        editor->cursor_col_--;
    }
    editor->put_line();
    
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
}
```

#### After (refactored test):
```cpp
TEST_F(BasicEditingTest, BackspaceMiddleOfLine) {
    CreateLine(0, "Hello World");
    LoadLine(0);
    editor->cursor_col_ = 6;
    editor->wksp_->view.basecol = 0;
    
    // Call actual method
    editor->edit_backspace();
    
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5);
}
```

### Tasks for Phase 3

- [x] Update all BACKSPACE tests to call `edit_backspace()`
- [x] Update all DELETE tests to call `edit_delete()`
- [x] Update all ENTER tests to call `edit_enter()`
- [x] Update all TAB tests to call `edit_tab()`
- [x] Update all character insertion tests to call `edit_insert_char()`
- [x] Update all character overwrite tests to call `edit_insert_char()`
- [x] Updated tests/basic_editing_test.cpp (22 tests)
- [x] Updated tests/horizontal_scroll_editing_test.cpp (28 tests)
- [x] LoadLine() properly called before each edit operation
- [x] Verify all 50 tests still pass - ALL TESTS PASS ✅
- [x] No missing edge cases - comprehensive coverage maintained

---

## Phase 4: Add Integration Tests (Optional)

### Goal
Test the complete flow from key press to editing action via `handle_key_edit()`.

### New Test File: tests/key_handling_integration_test.cpp

Create tests that:
1. Set up editor state (file content, cursor position, scroll position)
2. Call `handle_key_edit()` with specific key codes
3. Verify the resulting state (line content, cursor position, etc.)

#### Example Integration Tests

```cpp
TEST_F(KeyHandlingIntegrationTest, BackspaceKey) {
    CreateLine(0, "Hello World");
    editor->cursor_line_ = 0;
    editor->cursor_col_ = 6;
    editor->wksp_->view.basecol = 0;
    editor->wksp_->view.topline = 0;
    
    // Simulate backspace key press
    editor->handle_key_edit(KEY_BACKSPACE);
    
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5);
}

TEST_F(KeyHandlingIntegrationTest, CharacterInsertionKey) {
    CreateLine(0, "Helo");
    editor->cursor_line_ = 0;
    editor->cursor_col_ = 2;
    editor->insert_mode_ = true;
    
    // Simulate 'l' key press
    editor->handle_key_edit('l');
    
    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 3);
}
```

### Tasks for Phase 4

- [ ] Create tests/key_handling_integration_test.cpp
- [ ] Add test fixture similar to BasicEditingTest
- [ ] Add integration tests for BACKSPACE key
- [ ] Add integration tests for DELETE key
- [ ] Add integration tests for ENTER key
- [ ] Add integration tests for TAB key
- [ ] Add integration tests for character keys
- [ ] Add integration tests for navigation keys (arrow keys, etc.)
- [ ] Add tests for mode switching (insert vs overwrite)
- [ ] Add tests for special key combinations (Ctrl keys, F keys)
- [ ] Add to CMakeLists.txt
- [ ] Verify all integration tests pass

---

## Phase 5: Verification and Documentation

### Goal
Ensure the refactoring is complete, correct, and well-documented.

### Tasks for Phase 5

- [ ] Build project: `make clean && make`
- [ ] Run all unit tests: `./build/tests/v_edit_tests`
- [ ] Verify all 50+ existing tests pass
- [ ] Run integration tests if added
- [ ] Manual testing of editor with real files
- [ ] Check for any regressions in functionality
- [ ] Update code comments in new methods
- [ ] Update COLUMN_INDEXING_FIX_SUMMARY.md if needed
- [ ] Create REFACTORING_SUMMARY.md documenting the changes
- [ ] Git commit with clear message

---

## Implementation Order

1. **Phase 1** - Extract backend methods (foundation)
2. **Phase 2** - Refactor handle_key_edit() (integration)
3. **Phase 3** - Update existing unit tests (validation)
4. **Phase 4** - Add integration tests (optional, comprehensive testing)
5. **Phase 5** - Verification and documentation (completion)

---

## Key Decisions

### Where to Implement New Methods?

**Option A**: Keep in input.cpp (with handle_key_edit)
- ✅ Pro: All editing logic in one file
- ✅ Pro: Easy to see relationship with key handling
- ❌ Con: input.cpp gets larger

**Option B**: Move to edit.cpp (with other editing operations)
- ✅ Pro: Groups editing operations together
- ✅ Pro: input.cpp stays focused on input handling
- ❌ Con: Splits related code across files

**Recommendation**: Use edit.cpp - it already has editing operations like `splitline()` and `combineline()`, so the new methods fit naturally there.

### Method Signatures

All methods operate on the current editor state:
- Use existing member variables (`current_line_`, `cursor_col_`, etc.)
- No parameters needed (state is implicit)
- Return `void` (side effects on state)

This matches the existing pattern used by other Editor methods.

### Testing Strategy

1. **Unit tests** test the extracted methods directly
2. **Integration tests** test the complete key handling flow
3. Both types provide comprehensive coverage

---

## Expected Outcomes

✅ **Better separation of concerns** - UI routing vs editing logic  
✅ **More testable code** - Can test editing operations independently  
✅ **Clearer code** - Each operation has its own well-named method  
✅ **Better test coverage** - Tests exercise actual production code  
✅ **Easier maintenance** - Changes to editing logic are localized  
✅ **No functional changes** - Behavior remains identical  

---

## Notes

- The refactoring should be **behavior-preserving** - no functional changes
- All existing tests should continue to pass after Phase 1 and 2
- Only test updates in Phase 3 will cause tests to change
- Git commits should be made after each phase for easy rollback if needed
- Consider adding debug logging in new methods during development

---

## Status: Ready for Implementation

This plan is ready to execute. Proceed with Phase 1 when ready.
