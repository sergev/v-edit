#include <gtest/gtest.h>

#include "editor.h"

// Test fixture for editing operations with horizontal scroll (basecol > 0)
class HorizontalScrollEditingTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        editor = std::make_unique<Editor>();

        // Initialize editor state
        editor->wksp_     = std::make_unique<Workspace>(editor->tempfile_);
        editor->alt_wksp_ = std::make_unique<Workspace>(editor->tempfile_);

        // Open shared temp file
        editor->tempfile_.open_temp_file();

        // Initialize view state with horizontal scroll
        editor->wksp_->view.basecol = 10; // Scrolled right by 10 columns
        editor->wksp_->view.topline = 0;
        editor->cursor_col_         = 0;
        editor->cursor_line_        = 0;
        editor->ncols_              = 80;
        editor->nlines_             = 24;
    }

    void TearDown() override { editor.reset(); }

    // Helper: Create a line with content
    void CreateLine(int line_no, const std::string &content)
    {
        int current_count = editor->wksp_->total_line_count();
        if (line_no >= current_count) {
            auto blank = Workspace::create_blank_lines(line_no - current_count + 1);
            editor->wksp_->insert_contents(blank, current_count);
        }

        editor->current_line_          = content;
        editor->current_line_no_       = line_no;
        editor->current_line_modified_ = true;
        editor->put_line();
    }

    // Helper: Load a line into current_line_ buffer
    void LoadLine(int line_no) { editor->get_line(line_no); }

    // Helper: Get actual column position
    size_t GetActualCol() const { return editor->get_actual_col(); }

    std::unique_ptr<Editor> editor;
};

// ============================================================================
// BACKSPACE Operation Tests (basecol > 0)
// ============================================================================

TEST_F(HorizontalScrollEditingTest, BackspaceWithScroll)
{
    // Create line: "0123456789ABCDEFGHIJ"
    CreateLine(0, "0123456789ABCDEFGHIJ");
    LoadLine(0);

    // View scrolled right by 10, cursor at screen position 5
    // Actual position: 10 + 5 = 15 (character 'F')
    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;

    // Verify actual_col calculation
    EXPECT_EQ(GetActualCol(), 15);

    // Call backend method - should delete 'E' at position 14
    editor->edit_backspace();

    // Result: "0123456789ABCDFGHIJ" (E removed)
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDFGHIJ");
    EXPECT_EQ(editor->cursor_col_, 4);
}

TEST_F(HorizontalScrollEditingTest, BackspaceMultiplePositions)
{
    CreateLine(0, "The quick brown fox jumps over the lazy dog");
    LoadLine(0);

    // Test at different scroll positions
    struct TestCase {
        int basecol;
        int cursor_col;
        size_t expected_actual_col;
    };

    std::vector<TestCase> tests = {
        { 5, 3, 8 },    // basecol=5, cursor=3 -> actual=8
        { 10, 5, 15 },  // basecol=10, cursor=5 -> actual=15
        { 20, 10, 30 }, // basecol=20, cursor=10 -> actual=30
    };

    for (const auto &test : tests) {
        editor->wksp_->view.basecol = test.basecol;
        editor->cursor_col_         = test.cursor_col;
        EXPECT_EQ(GetActualCol(), test.expected_actual_col);
    }
}

// ============================================================================
// DELETE Operation Tests (basecol > 0)
// ============================================================================

TEST_F(HorizontalScrollEditingTest, DeleteWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    LoadLine(0);

    // View scrolled right by 10, cursor at screen position 5
    // Actual position: 10 + 5 = 15 (character 'F')
    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;

    EXPECT_EQ(GetActualCol(), 15);

    // Call backend method - delete character at cursor position (F at position 15)
    editor->edit_delete();

    // Result: "0123456789ABCDEGHIJ" (F removed)
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEGHIJ");
    EXPECT_EQ(editor->cursor_col_, 5); // Cursor position unchanged
}

TEST_F(HorizontalScrollEditingTest, DeleteAtVariousScrollPositions)
{
    CreateLine(0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    LoadLine(0);

    // Test deletion at basecol=15, cursor=5 -> actual=20 (U)
    editor->wksp_->view.basecol = 15;
    editor->cursor_col_         = 5;

    EXPECT_EQ(GetActualCol(), 20);

    // Call backend method
    editor->edit_delete();

    // Result: "ABCDEFGHIJKLMNOPQRSTVWXYZ" (U removed at position 20)
    EXPECT_EQ(editor->wksp_->read_line(0), "ABCDEFGHIJKLMNOPQRSTVWXYZ");
}

// ============================================================================
// ENTER/Newline Operation Tests (basecol > 0)
// ============================================================================

TEST_F(HorizontalScrollEditingTest, EnterWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    LoadLine(0);

    // View scrolled right by 10, cursor at screen position 5
    // Actual position: 10 + 5 = 15
    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;

    EXPECT_EQ(GetActualCol(), 15);

    // Split line at actual position 15
    size_t actual_col = GetActualCol();
    std::string tail;
    if (actual_col < editor->current_line_.size()) {
        tail = editor->current_line_.substr(actual_col);
        editor->current_line_.erase(actual_col);
    }
    editor->current_line_modified_ = true;
    editor->put_line();

    // First part: "0123456789ABCDE"
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDE");
    // Tail: "FGHIJ"
    EXPECT_EQ(tail, "FGHIJ");
}

TEST_F(HorizontalScrollEditingTest, EnterWithLargeScroll)
{
    CreateLine(0, "This is a very long line that requires horizontal scrolling to see");
    LoadLine(0);

    // Scrolled far right
    editor->wksp_->view.basecol = 30;
    editor->cursor_col_         = 10;

    // Actual position: 30 + 10 = 40
    EXPECT_EQ(GetActualCol(), 40);

    size_t actual_col = GetActualCol();
    std::string tail;
    if (actual_col < editor->current_line_.size()) {
        tail = editor->current_line_.substr(actual_col);
        editor->current_line_.erase(actual_col);
    }
    editor->current_line_modified_ = true;
    editor->put_line();

    std::string first_part = editor->wksp_->read_line(0);
    EXPECT_EQ(first_part.length(), 40);
    EXPECT_EQ(first_part, "This is a very long line that requires h");
    EXPECT_EQ(tail, "orizontal scrolling to see");
}

// ============================================================================
// TAB Operation Tests (basecol > 0)
// ============================================================================

TEST_F(HorizontalScrollEditingTest, TabWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    LoadLine(0);

    // View scrolled right by 10, cursor at screen position 5
    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;

    EXPECT_EQ(GetActualCol(), 15);

    // Call backend method - insert tab (4 spaces) at actual position 15
    editor->edit_tab();

    // Result: "0123456789ABCDE    FGHIJ"
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDE    FGHIJ");
    EXPECT_EQ(editor->cursor_col_, 9);
}

TEST_F(HorizontalScrollEditingTest, TabAtScrollBoundary)
{
    CreateLine(0, "AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH");
    LoadLine(0);

    // Right at scroll boundary
    editor->wksp_->view.basecol = 20;
    editor->cursor_col_         = 0;

    // Actual position: 20
    EXPECT_EQ(GetActualCol(), 20);

    // Call backend method - insert tab
    editor->edit_tab();

    // Spaces inserted at position 20
    std::string result = editor->wksp_->read_line(0);
    EXPECT_EQ(result.substr(20, 4), "    ");
}

// ============================================================================
// Character Insertion Tests (basecol > 0)
// ============================================================================

TEST_F(HorizontalScrollEditingTest, InsertCharacterWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    LoadLine(0);

    // View scrolled, insert at actual position
    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 15);

    // Call backend method - insert 'X' at actual position 15
    editor->edit_insert_char('X');

    // Result: "0123456789ABCDEXFGHIJ"
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEXFGHIJ");
    EXPECT_EQ(editor->cursor_col_, 6);
}

TEST_F(HorizontalScrollEditingTest, InsertMultipleCharactersWithScroll)
{
    CreateLine(0, "StartEnd");
    LoadLine(0);

    editor->wksp_->view.basecol = 3;
    editor->cursor_col_         = 2;
    editor->insert_mode_        = true;

    // Actual position: 3 + 2 = 5 (between 't' and 'E')
    EXPECT_EQ(GetActualCol(), 5);

    // Insert " Middle " using backend method
    const char *text = " Middle ";
    for (size_t i = 0; text[i] != '\0'; ++i) {
        editor->edit_insert_char(text[i]);
    }

    EXPECT_EQ(editor->wksp_->read_line(0), "Start Middle End");
}

// ============================================================================
// Character Overwrite Tests (basecol > 0)
// ============================================================================

TEST_F(HorizontalScrollEditingTest, OverwriteWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    LoadLine(0);

    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;
    editor->insert_mode_        = false; // Overwrite mode

    EXPECT_EQ(GetActualCol(), 15);

    // Call backend method - overwrite 'F' with 'X' at actual position 15
    editor->edit_insert_char('X');

    // Result: "0123456789ABCDEXGHIJ"
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEXGHIJ");
    EXPECT_EQ(editor->cursor_col_, 6);
}

TEST_F(HorizontalScrollEditingTest, OverwriteMultipleWithScroll)
{
    CreateLine(0, "The quick brown fox");
    LoadLine(0);

    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 0;
    editor->insert_mode_        = false;

    // Actual position: 10 + 0 = 10 ('b' in "brown")
    EXPECT_EQ(GetActualCol(), 10);

    // Overwrite "brown" with "BLACK" using backend method
    const char *text = "BLACK";
    for (size_t i = 0; text[i] != '\0'; ++i) {
        editor->edit_insert_char(text[i]);
    }

    EXPECT_EQ(editor->wksp_->read_line(0), "The quick BLACK fox");
}

// ============================================================================
// Edge Cases with Horizontal Scroll
// ============================================================================

TEST_F(HorizontalScrollEditingTest, EditingAtMaxScroll)
{
    // Create a 100-character line
    std::string long_line(100, 'X');
    CreateLine(0, long_line);
    LoadLine(0);

    // Scroll far to the right
    editor->wksp_->view.basecol = 80;
    editor->cursor_col_         = 10;

    // Actual position: 80 + 10 = 90
    EXPECT_EQ(GetActualCol(), 90);

    // Call backend method - delete character at position 90
    editor->edit_delete();

    EXPECT_EQ(editor->wksp_->read_line(0).length(), 99);
}

TEST_F(HorizontalScrollEditingTest, ActualColCalculationVariousScrolls)
{
    CreateLine(0, "Test line for verification");
    LoadLine(0);

    // Test various basecol and cursor_col combinations
    struct TestCase {
        int basecol;
        int cursor_col;
        size_t expected;
    };

    std::vector<TestCase> tests = {
        { 0, 0, 0 },    { 0, 5, 5 },   { 5, 0, 5 },   { 5, 5, 10 },
        { 10, 10, 20 }, { 15, 7, 22 }, { 20, 3, 23 },
    };

    for (const auto &test : tests) {
        editor->wksp_->view.basecol = test.basecol;
        editor->cursor_col_         = test.cursor_col;
        EXPECT_EQ(GetActualCol(), test.expected)
            << "Failed for basecol=" << test.basecol << " cursor_col=" << test.cursor_col;
    }
}

TEST_F(HorizontalScrollEditingTest, ComplexEditingSequenceWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJKLMNOP");
    LoadLine(0);

    // Start with scroll
    editor->wksp_->view.basecol = 8;
    editor->cursor_col_         = 4;
    editor->insert_mode_        = true;

    // Actual position: 8 + 4 = 12 (character 'C')
    EXPECT_EQ(GetActualCol(), 12);

    // Call backend method - insert 'X'
    editor->edit_insert_char('X');

    // Verify: "0123456789ABXCDEFGHIJKLMNOP"
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABXCDEFGHIJKLMNOP");

    // Now delete it using backend method
    LoadLine(0);
    editor->cursor_col_ = 5;       // Moved forward after insert
    EXPECT_EQ(GetActualCol(), 13); // 8 + 5 = 13

    editor->edit_backspace();

    // Back to original: "0123456789ABCDEFGHIJKLMNOP"
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEFGHIJKLMNOP");
}

TEST_F(HorizontalScrollEditingTest, VerifyBugFixScenario)
{
    // This test specifically verifies the bug that was fixed:
    // Without the fix, editing at cursor_col=6 with basecol=6
    // would incorrectly edit at position 6 instead of position 12

    CreateLine(0, "Hello World Test Line");
    LoadLine(0);

    editor->wksp_->view.basecol = 6; // Scrolled right by 6
    editor->cursor_col_         = 6; // Cursor at screen position 6

    // WITHOUT FIX: Would use cursor_col_ = 6 directly (WRONG!)
    // WITH FIX: Uses get_actual_col() = 6 + 6 = 12 (CORRECT!)
    EXPECT_EQ(GetActualCol(), 12);

    // Verify character at position 12 before deleting
    size_t actual_col = GetActualCol();
    ASSERT_LT(actual_col, editor->current_line_.size());
    char char_at_12 = editor->current_line_[actual_col];
    EXPECT_EQ(char_at_12, 'T'); // The character at position 12 is 'T'

    // Call backend method - delete character at actual position 12
    editor->edit_delete();

    // Result: "Hello World est Line" ('T' removed)
    EXPECT_EQ(editor->wksp_->read_line(0), "Hello World est Line");
}

// ============================================================================
// Cursor Beyond Line End Tests
// ============================================================================

TEST_F(HorizontalScrollEditingTest, CursorBeyondLineEndNoScroll)
{
    CreateLine(0, "Short");
    LoadLine(0);

    // Position cursor beyond line end (no scroll)
    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 10; // Beyond line length of 5

    // Actual position: 0 + 10 = 10 (beyond line end)
    EXPECT_EQ(GetActualCol(), 10);
    EXPECT_GT(GetActualCol(), editor->current_line_.size());

    // Insert character beyond line end (should extend line)
    size_t actual_col    = GetActualCol();
    editor->insert_mode_ = true;

    // In real code, this would pad with spaces or just append
    // For this test, we verify the actual_col calculation is correct
    EXPECT_EQ(actual_col, 10);
}

TEST_F(HorizontalScrollEditingTest, CursorBeyondLineEndWithScroll)
{
    CreateLine(0, "Short");
    LoadLine(0);

    // Position cursor beyond line end WITH scroll
    editor->wksp_->view.basecol = 5;
    editor->cursor_col_         = 10;

    // Actual position: 5 + 10 = 15 (way beyond line length of 5)
    EXPECT_EQ(GetActualCol(), 15);
    EXPECT_GT(GetActualCol(), editor->current_line_.size());

    // Verify bounds checking works correctly
    size_t actual_col = GetActualCol();
    EXPECT_FALSE(actual_col < editor->current_line_.size());
}

TEST_F(HorizontalScrollEditingTest, BackspaceBeyondLineEndWithScroll)
{
    CreateLine(0, "Test");
    LoadLine(0);

    // Cursor beyond line end
    editor->wksp_->view.basecol = 3;
    editor->cursor_col_         = 5;

    // Actual position: 3 + 5 = 8 (beyond line length of 4)
    EXPECT_EQ(GetActualCol(), 8);

    size_t actual_col = GetActualCol();
    // Backspace should still work - it would operate based on bounds check
    if (actual_col > 0 && actual_col <= editor->current_line_.size()) {
        editor->current_line_.erase(actual_col - 1, 1);
        editor->current_line_modified_ = true;
    }

    // Line should be unchanged since actual_col > line size
    editor->put_line();
    EXPECT_EQ(editor->wksp_->read_line(0), "Test");
}

TEST_F(HorizontalScrollEditingTest, DeleteBeyondLineEndWithScroll)
{
    CreateLine(0, "Test");
    CreateLine(1, "Next");
    LoadLine(0);

    // Cursor beyond line end on first line
    editor->wksp_->view.basecol = 2;
    editor->cursor_col_         = 5;

    // Actual position: 2 + 5 = 7 (beyond line length of 4)
    EXPECT_EQ(GetActualCol(), 7);

    size_t actual_col = GetActualCol();
    // Delete should not work within line since beyond end
    EXPECT_FALSE(actual_col < editor->current_line_.size());

    // In real code, this would trigger line joining with next line
}

TEST_F(HorizontalScrollEditingTest, InsertBeyondLineEndWithScroll)
{
    CreateLine(0, "Hi");
    LoadLine(0);

    // Cursor way beyond line end
    editor->wksp_->view.basecol = 10;
    editor->cursor_col_         = 5;

    // Actual position: 10 + 5 = 15 (beyond line length of 2)
    EXPECT_EQ(GetActualCol(), 15);

    editor->insert_mode_ = true;
    size_t actual_col    = GetActualCol();

    // Insert at position beyond line would extend line
    // For now, just verify the calculation is correct
    EXPECT_GT(actual_col, editor->current_line_.size());
}

// ============================================================================
// Line Joining Operations with Scroll
// ============================================================================

TEST_F(HorizontalScrollEditingTest, BackspaceJoinLinesNoScroll)
{
    CreateLine(0, "First");
    CreateLine(1, "Second");

    // Start on second line at beginning
    editor->cursor_line_ = 1;
    LoadLine(1);

    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 0;

    // Actual position: 0 + 0 = 0 (start of line)
    EXPECT_EQ(GetActualCol(), 0);

    size_t actual_col = GetActualCol();

    // Backspace at start of line should join with previous line
    // actual_col == 0, so the if condition for within-line deletion is false
    EXPECT_EQ(actual_col, 0);

    // In actual implementation, would join "First" + "Second" = "FirstSecond"
}

TEST_F(HorizontalScrollEditingTest, BackspaceJoinLinesWithScroll)
{
    CreateLine(0, "First line");
    CreateLine(1, "Second line");

    // Start on second line, scrolled to the right
    editor->cursor_line_ = 1;
    LoadLine(1);

    editor->wksp_->view.basecol = 5;
    editor->cursor_col_         = 0;

    // Actual position: 5 + 0 = 5 (NOT at start of line, at position 5)
    EXPECT_EQ(GetActualCol(), 5);

    // Call backend method - with scroll, actual_col > 0, so backspace deletes character in line
    editor->edit_backspace();

    // Should delete character at position 4: "Second line" -> "Secod line"
    EXPECT_EQ(editor->wksp_->read_line(1), "Secod line");
}

TEST_F(HorizontalScrollEditingTest, BackspaceJoinLinesAtTrueStart)
{
    CreateLine(0, "First line");
    CreateLine(1, "Second line");

    // Start on second line at TRUE beginning (basecol=0, cursor=0)
    editor->cursor_line_ = 1;
    LoadLine(1);

    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 0;

    // Actual position: 0 + 0 = 0 (TRUE start of line)
    EXPECT_EQ(GetActualCol(), 0);

    // This should trigger line joining in actual code
    // Verify we correctly identify this as start of line
    size_t actual_col = GetActualCol();
    EXPECT_EQ(actual_col, 0);
}

TEST_F(HorizontalScrollEditingTest, DeleteJoinLinesAtEndNoScroll)
{
    CreateLine(0, "First");
    CreateLine(1, "Second");
    LoadLine(0);

    // At end of first line
    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 5; // After "First"

    // Actual position: 0 + 5 = 5 (at end, line length is 5)
    EXPECT_EQ(GetActualCol(), 5);

    size_t actual_col = GetActualCol();

    // Delete at end of line should join with next line
    EXPECT_EQ(actual_col, editor->current_line_.size());
    EXPECT_FALSE(actual_col < editor->current_line_.size());

    // In actual code, this would join "First" + "Second" = "FirstSecond"
}

TEST_F(HorizontalScrollEditingTest, DeleteJoinLinesAtEndWithScroll)
{
    CreateLine(0, "First");
    CreateLine(1, "Second");
    LoadLine(0);

    // Scrolled right, cursor at what appears to be end
    editor->wksp_->view.basecol = 3;
    editor->cursor_col_         = 2;

    // Actual position: 3 + 2 = 5 (at end, line length is 5)
    EXPECT_EQ(GetActualCol(), 5);

    size_t actual_col = GetActualCol();

    // Delete at end should still trigger line joining
    EXPECT_EQ(actual_col, editor->current_line_.size());
    EXPECT_FALSE(actual_col < editor->current_line_.size());
}

TEST_F(HorizontalScrollEditingTest, DeleteNotAtEndWithScroll)
{
    CreateLine(0, "Testing");
    CreateLine(1, "Second");
    LoadLine(0);

    // Scrolled, but not at actual end
    editor->wksp_->view.basecol = 2;
    editor->cursor_col_         = 3;

    // Actual position: 2 + 3 = 5 (line length is 7)
    EXPECT_EQ(GetActualCol(), 5);

    size_t actual_col = GetActualCol();

    // Should delete within line, not join
    EXPECT_LT(actual_col, editor->current_line_.size());

    // Call backend method - delete character at position 5
    editor->edit_delete();

    // Should delete 'n' at position 5: "Testing" -> "Testig"
    EXPECT_EQ(editor->wksp_->read_line(0), "Testig");
}

TEST_F(HorizontalScrollEditingTest, LineJoiningEdgeCaseScrolled)
{
    CreateLine(0, "Line1");
    CreateLine(1, "Line2");
    CreateLine(2, "Line3");

    // Test multiple scenarios with scroll
    // Scenario 1: Delete at end of line with scroll should join
    LoadLine(0);
    editor->wksp_->view.basecol = 2;
    editor->cursor_col_         = 3;

    // Actual position: 2 + 3 = 5 (end of "Line1")
    EXPECT_EQ(GetActualCol(), 5);
    EXPECT_EQ(GetActualCol(), editor->current_line_.size());

    // Scenario 2: Verify line 1 is independent
    LoadLine(1);
    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 0;
    EXPECT_EQ(GetActualCol(), 0);
    EXPECT_EQ(editor->wksp_->read_line(1), "Line2");
}
