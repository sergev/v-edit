#include <gtest/gtest.h>

#include "EditorDriver.h"

// ============================================================================
// BACKSPACE Operation Tests (basecol = 0)
// ============================================================================

TEST_F(EditorDriver, BasicEditingBackspaceMiddleOfLine)
{
    // Create a line: "Hello World"
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at 'W' (column 6)
    editor->cursor_col_         = 6;
    editor->wksp_->view.basecol = 0;

    // Verify actual_col calculation
    EXPECT_EQ(GetActualCol(), 6);

    // Call backend method - should delete space before 'W'
    editor->edit_backspace();

    // Verify result: "HelloWorld"
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5);
}

TEST_F(EditorDriver, BasicEditingBackspaceStartOfLine)
{
    CreateLine(0, "First");
    CreateLine(1, "Second");
    LoadLine(1);

    // Position cursor at start of line 1
    editor->cursor_col_         = 0;
    editor->cursor_line_        = 1;
    editor->wksp_->view.basecol = 0;

    // Verify actual_col is 0
    EXPECT_EQ(GetActualCol(), 0);

    // Should not delete within line (actual_col == 0)
    size_t actual_col = GetActualCol();
    EXPECT_EQ(actual_col, 0);

    // In actual code, this would trigger line joining
    // Here we just verify actual_col is correct
}

TEST_F(EditorDriver, BasicEditingBackspaceEndOfLine)
{
    CreateLine(0, "Test");
    LoadLine(0);

    // Position cursor at end (column 4)
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 4);

    // Call backend method - delete last character
    editor->edit_backspace();

    EXPECT_EQ(editor->wksp_->read_line(0), "Tes");
    EXPECT_EQ(editor->cursor_col_, 3);
}

// ============================================================================
// DELETE Operation Tests (basecol = 0)
// ============================================================================

TEST_F(EditorDriver, BasicEditingDeleteMiddleOfLine)
{
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at space (column 5)
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Call backend method - delete character at cursor
    editor->edit_delete();

    // Result: "HelloWorld"
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5); // Cursor position unchanged
}

TEST_F(EditorDriver, BasicEditingDeleteAtEndOfLine)
{
    CreateLine(0, "Test");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 4);

    // Should not delete (at end of line)
    size_t actual_col = GetActualCol();
    EXPECT_EQ(actual_col, editor->current_line_.size());
    // In actual code, this would trigger line joining
}

TEST_F(EditorDriver, BasicEditingDeleteFirstCharacter)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 0);

    // Call backend method - delete first character
    editor->edit_delete();

    EXPECT_EQ(editor->wksp_->read_line(0), "ello");
    EXPECT_EQ(editor->cursor_col_, 0);
}

// ============================================================================
// ENTER/Newline Operation Tests (basecol = 0)
// ============================================================================

TEST_F(EditorDriver, BasicEditingEnterMiddleOfLine)
{
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at space (column 5)
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Split line at cursor
    size_t actual_col = GetActualCol();
    std::string tail;
    if (actual_col < editor->current_line_.size()) {
        tail = editor->current_line_.substr(actual_col);
        editor->current_line_.erase(actual_col);
    }
    editor->current_line_modified_ = true;
    editor->put_line();

    // Verify first part: "Hello"
    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");

    // In actual code, tail would be inserted as new line
    EXPECT_EQ(tail, " World");
}

TEST_F(EditorDriver, BasicEditingEnterAtStartOfLine)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 0);

    // Split at start
    size_t actual_col = GetActualCol();
    std::string tail;
    if (actual_col < editor->current_line_.size()) {
        tail = editor->current_line_.substr(actual_col);
        editor->current_line_.erase(actual_col);
    }
    editor->current_line_modified_ = true;
    editor->put_line();

    // First line becomes empty
    EXPECT_EQ(editor->wksp_->read_line(0), "");
    // Tail contains everything
    EXPECT_EQ(tail, "Hello");
}

TEST_F(EditorDriver, BasicEditingEnterAtEndOfLine)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Split at end
    size_t actual_col = GetActualCol();
    std::string tail;
    if (actual_col < editor->current_line_.size()) {
        tail = editor->current_line_.substr(actual_col);
        editor->current_line_.erase(actual_col);
    }
    editor->current_line_modified_ = true;
    editor->put_line();

    // First line unchanged
    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    // Tail is empty
    EXPECT_TRUE(tail.empty());
}

// ============================================================================
// TAB Operation Tests (basecol = 0)
// ============================================================================

TEST_F(EditorDriver, BasicEditingTabAtStart)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 0);

    // Call backend method - insert tab (4 spaces)
    editor->edit_tab();

    EXPECT_EQ(editor->wksp_->read_line(0), "    Hello");
    EXPECT_EQ(editor->cursor_col_, 4);
}

TEST_F(EditorDriver, BasicEditingTabMiddleOfLine)
{
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at space (column 5)
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Call backend method - insert tab
    editor->edit_tab();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello     World");
    EXPECT_EQ(editor->cursor_col_, 9);
}

TEST_F(EditorDriver, BasicEditingTabAtEnd)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Call backend method - insert tab
    editor->edit_tab();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello    ");
    EXPECT_EQ(editor->cursor_col_, 9);
}

// ============================================================================
// Character Insertion Tests (basecol = 0)
// ============================================================================

TEST_F(EditorDriver, BasicEditingInsertCharacterAtStart)
{
    CreateLine(0, "ello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 0);

    // Call backend method - insert 'H'
    editor->edit_insert_char('H');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 1);
}

TEST_F(EditorDriver, BasicEditingInsertCharacterMiddle)
{
    CreateLine(0, "Helo");
    LoadLine(0);

    // Position cursor at 'l' (column 2)
    editor->cursor_col_         = 2;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 2);

    // Call backend method - insert 'l'
    editor->edit_insert_char('l');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 3);
}

TEST_F(EditorDriver, BasicEditingInsertCharacterAtEnd)
{
    CreateLine(0, "Hell");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 4);

    // Call backend method - insert 'o'
    editor->edit_insert_char('o');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 5);
}

// ============================================================================
// Character Overwrite Tests (basecol = 0)
// ============================================================================

TEST_F(EditorDriver, BasicEditingOverwriteCharacterMiddle)
{
    CreateLine(0, "Hxllo");
    LoadLine(0);

    // Position cursor at 'x' (column 1)
    editor->cursor_col_         = 1;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = false; // Overwrite mode

    EXPECT_EQ(GetActualCol(), 1);

    // Call backend method - overwrite with 'e'
    editor->edit_insert_char('e');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 2);
}

TEST_F(EditorDriver, BasicEditingOverwriteAtEnd)
{
    CreateLine(0, "Hell");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = false;

    EXPECT_EQ(GetActualCol(), 4);

    // Call backend method - overwrite (extends line since at end)
    editor->edit_insert_char('o');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 5);
}

TEST_F(EditorDriver, BasicEditingOverwriteFirstCharacter)
{
    CreateLine(0, "xello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = false;

    EXPECT_EQ(GetActualCol(), 0);

    // Call backend method - overwrite with 'H'
    editor->edit_insert_char('H');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 1);
}

// ============================================================================
// Edge Cases and Complex Scenarios
// ============================================================================

TEST_F(EditorDriver, BasicEditingMultipleOperationsSequence)
{
    // Start with empty line
    CreateLine(0, "");
    LoadLine(0);

    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    // Insert "Test" using backend method
    const char *text = "Test";
    for (size_t i = 0; text[i] != '\0'; ++i) {
        editor->edit_insert_char(text[i]);
    }

    EXPECT_EQ(editor->wksp_->read_line(0), "Test");
    EXPECT_EQ(editor->cursor_col_, 4);

    // Delete last character using backend method
    LoadLine(0);
    editor->edit_backspace();

    EXPECT_EQ(editor->wksp_->read_line(0), "Tes");
    EXPECT_EQ(editor->cursor_col_, 3);
}

TEST_F(EditorDriver, BasicEditingEmptyLineOperations)
{
    CreateLine(0, "");
    LoadLine(0);

    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    // Verify actual_col is 0
    EXPECT_EQ(GetActualCol(), 0);
    EXPECT_EQ(editor->current_line_.size(), 0);

    // Insert character into empty line using backend method
    editor->insert_mode_ = true;
    editor->edit_insert_char('A');

    EXPECT_EQ(editor->wksp_->read_line(0), "A");
    EXPECT_EQ(editor->cursor_col_, 1);
}

TEST_F(EditorDriver, BasicEditingLongLineEditing)
{
    // Create a long line
    std::string long_line(100, 'x');
    CreateLine(0, long_line);
    LoadLine(0);

    // Edit in the middle
    editor->cursor_col_         = 50;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 50);

    // Delete character at position 50 using backend method
    editor->edit_delete();

    // Result should be 99 characters
    EXPECT_EQ(editor->wksp_->read_line(0).size(), 99);
    EXPECT_EQ(editor->cursor_col_, 50);
}

TEST_F(EditorDriver, BasicEditingActualColMatchesCursorColWhenBasecolZero)
{
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Test at various cursor positions
    for (int col = 0; col <= 11; ++col) {
        editor->cursor_col_         = col;
        editor->wksp_->view.basecol = 0;

        // When basecol is 0, actual_col should equal cursor_col
        EXPECT_EQ(GetActualCol(), static_cast<size_t>(col));
    }
}
