#include <gtest/gtest.h>

#include "editor.h"

// Test fixture for basic editing operations with basecol = 0
class BasicEditingTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        editor = std::make_unique<Editor>();

        // Initialize editor state
        editor->wksp_     = std::make_unique<Workspace>(editor->tempfile_);
        editor->alt_wksp_ = std::make_unique<Workspace>(editor->tempfile_);

        // Open shared temp file
        editor->tempfile_.open_temp_file();

        // Initialize view state (no horizontal scroll)
        editor->wksp_->view.basecol = 0;
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
        // Ensure enough blank lines exist
        int current_count = editor->wksp_->total_line_count();
        if (line_no >= current_count) {
            auto blank = Workspace::create_blank_lines(line_no - current_count + 1);
            editor->wksp_->insert_contents(blank, current_count);
        }

        // Set the content
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
// BACKSPACE Operation Tests (basecol = 0)
// ============================================================================

TEST_F(BasicEditingTest, BackspaceMiddleOfLine)
{
    // Create a line: "Hello World"
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at 'W' (column 6)
    editor->cursor_col_         = 6;
    editor->wksp_->view.basecol = 0;

    // Verify actual_col calculation
    EXPECT_EQ(GetActualCol(), 6);

    // Simulate backspace - should delete space before 'W'
    size_t actual_col = GetActualCol();
    if (actual_col > 0 && actual_col <= editor->current_line_.size()) {
        editor->current_line_.erase(actual_col - 1, 1);
        editor->current_line_modified_ = true;
        editor->cursor_col_--;
    }
    editor->put_line();

    // Verify result: "HelloWorld"
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5);
}

TEST_F(BasicEditingTest, BackspaceStartOfLine)
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

TEST_F(BasicEditingTest, BackspaceEndOfLine)
{
    CreateLine(0, "Test");
    LoadLine(0);

    // Position cursor at end (column 4)
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 4);

    // Delete last character
    size_t actual_col = GetActualCol();
    if (actual_col > 0 && actual_col <= editor->current_line_.size()) {
        editor->current_line_.erase(actual_col - 1, 1);
        editor->current_line_modified_ = true;
        editor->cursor_col_--;
    }
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Tes");
    EXPECT_EQ(editor->cursor_col_, 3);
}

// ============================================================================
// DELETE Operation Tests (basecol = 0)
// ============================================================================

TEST_F(BasicEditingTest, DeleteMiddleOfLine)
{
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at space (column 5)
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Delete character at cursor
    size_t actual_col = GetActualCol();
    if (actual_col < editor->current_line_.size()) {
        editor->current_line_.erase(actual_col, 1);
        editor->current_line_modified_ = true;
    }
    editor->put_line();

    // Result: "HelloWorld"
    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5); // Cursor position unchanged
}

TEST_F(BasicEditingTest, DeleteAtEndOfLine)
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

TEST_F(BasicEditingTest, DeleteFirstCharacter)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 0);

    // Delete first character
    size_t actual_col = GetActualCol();
    if (actual_col < editor->current_line_.size()) {
        editor->current_line_.erase(actual_col, 1);
        editor->current_line_modified_ = true;
    }
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "ello");
    EXPECT_EQ(editor->cursor_col_, 0);
}

// ============================================================================
// ENTER/Newline Operation Tests (basecol = 0)
// ============================================================================

TEST_F(BasicEditingTest, EnterMiddleOfLine)
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

TEST_F(BasicEditingTest, EnterAtStartOfLine)
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

TEST_F(BasicEditingTest, EnterAtEndOfLine)
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

TEST_F(BasicEditingTest, TabAtStart)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 0);

    // Insert 4 spaces (tab)
    size_t actual_col = GetActualCol();
    editor->current_line_.insert(actual_col, 4, ' ');
    editor->current_line_modified_ = true;
    editor->cursor_col_ += 4;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "    Hello");
    EXPECT_EQ(editor->cursor_col_, 4);
}

TEST_F(BasicEditingTest, TabMiddleOfLine)
{
    CreateLine(0, "Hello World");
    LoadLine(0);

    // Position cursor at space (column 5)
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Insert 4 spaces
    size_t actual_col = GetActualCol();
    editor->current_line_.insert(actual_col, 4, ' ');
    editor->current_line_modified_ = true;
    editor->cursor_col_ += 4;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello     World");
    EXPECT_EQ(editor->cursor_col_, 9);
}

TEST_F(BasicEditingTest, TabAtEnd)
{
    CreateLine(0, "Hello");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 5);

    // Insert 4 spaces
    size_t actual_col = GetActualCol();
    editor->current_line_.insert(actual_col, 4, ' ');
    editor->current_line_modified_ = true;
    editor->cursor_col_ += 4;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello    ");
    EXPECT_EQ(editor->cursor_col_, 9);
}

// ============================================================================
// Character Insertion Tests (basecol = 0)
// ============================================================================

TEST_F(BasicEditingTest, InsertCharacterAtStart)
{
    CreateLine(0, "ello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 0);

    // Insert 'H'
    size_t actual_col = GetActualCol();
    editor->current_line_.insert(actual_col, 1, 'H');
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 1);
}

TEST_F(BasicEditingTest, InsertCharacterMiddle)
{
    CreateLine(0, "Helo");
    LoadLine(0);

    // Position cursor at 'l' (column 2)
    editor->cursor_col_         = 2;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 2);

    // Insert 'l'
    size_t actual_col = GetActualCol();
    editor->current_line_.insert(actual_col, 1, 'l');
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 3);
}

TEST_F(BasicEditingTest, InsertCharacterAtEnd)
{
    CreateLine(0, "Hell");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    EXPECT_EQ(GetActualCol(), 4);

    // Insert 'o'
    size_t actual_col = GetActualCol();
    editor->current_line_.insert(actual_col, 1, 'o');
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 5);
}

// ============================================================================
// Character Overwrite Tests (basecol = 0)
// ============================================================================

TEST_F(BasicEditingTest, OverwriteCharacterMiddle)
{
    CreateLine(0, "Hxllo");
    LoadLine(0);

    // Position cursor at 'x' (column 1)
    editor->cursor_col_         = 1;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = false; // Overwrite mode

    EXPECT_EQ(GetActualCol(), 1);

    // Overwrite with 'e'
    size_t actual_col = GetActualCol();
    if (actual_col < editor->current_line_.size()) {
        editor->current_line_[actual_col] = 'e';
    } else {
        editor->current_line_.push_back('e');
    }
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 2);
}

TEST_F(BasicEditingTest, OverwriteAtEnd)
{
    CreateLine(0, "Hell");
    LoadLine(0);

    // Position cursor at end
    editor->cursor_col_         = 4;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = false;

    EXPECT_EQ(GetActualCol(), 4);

    // Overwrite (extends line since at end)
    size_t actual_col = GetActualCol();
    if (actual_col < editor->current_line_.size()) {
        editor->current_line_[actual_col] = 'o';
    } else {
        editor->current_line_.push_back('o');
    }
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 5);
}

TEST_F(BasicEditingTest, OverwriteFirstCharacter)
{
    CreateLine(0, "xello");
    LoadLine(0);

    // Position cursor at start
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = false;

    EXPECT_EQ(GetActualCol(), 0);

    // Overwrite with 'H'
    size_t actual_col = GetActualCol();
    if (actual_col < editor->current_line_.size()) {
        editor->current_line_[actual_col] = 'H';
    } else {
        editor->current_line_.push_back('H');
    }
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 1);
}

// ============================================================================
// Edge Cases and Complex Scenarios
// ============================================================================

TEST_F(BasicEditingTest, MultipleOperationsSequence)
{
    // Start with empty line
    CreateLine(0, "");
    LoadLine(0);

    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;
    editor->insert_mode_        = true;

    // Insert "Test"
    const char *text = "Test";
    for (size_t i = 0; text[i] != '\0'; ++i) {
        size_t actual_col = GetActualCol();
        editor->current_line_.insert(actual_col, 1, text[i]);
        editor->cursor_col_++;
    }
    editor->current_line_modified_ = true;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Test");
    EXPECT_EQ(editor->cursor_col_, 4);

    // Delete last character
    LoadLine(0);
    size_t actual_col = GetActualCol();
    if (actual_col > 0 && actual_col <= editor->current_line_.size()) {
        editor->current_line_.erase(actual_col - 1, 1);
        editor->current_line_modified_ = true;
        editor->cursor_col_--;
    }
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "Tes");
    EXPECT_EQ(editor->cursor_col_, 3);
}

TEST_F(BasicEditingTest, EmptyLineOperations)
{
    CreateLine(0, "");
    LoadLine(0);

    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 0;

    // Verify actual_col is 0
    EXPECT_EQ(GetActualCol(), 0);
    EXPECT_EQ(editor->current_line_.size(), 0);

    // Insert character into empty line
    editor->insert_mode_ = true;
    size_t actual_col    = GetActualCol();
    editor->current_line_.insert(actual_col, 1, 'A');
    editor->current_line_modified_ = true;
    editor->cursor_col_++;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line(0), "A");
    EXPECT_EQ(editor->cursor_col_, 1);
}

TEST_F(BasicEditingTest, LongLineEditing)
{
    // Create a long line
    std::string long_line(100, 'x');
    CreateLine(0, long_line);
    LoadLine(0);

    // Edit in the middle
    editor->cursor_col_         = 50;
    editor->wksp_->view.basecol = 0;

    EXPECT_EQ(GetActualCol(), 50);

    // Delete character at position 50
    size_t actual_col = GetActualCol();
    if (actual_col < editor->current_line_.size()) {
        editor->current_line_.erase(actual_col, 1);
        editor->current_line_modified_ = true;
    }
    editor->put_line();

    // Result should be 99 characters
    EXPECT_EQ(editor->wksp_->read_line(0).size(), 99);
    EXPECT_EQ(editor->cursor_col_, 50);
}

TEST_F(BasicEditingTest, ActualColMatchesCursorColWhenBasecolZero)
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
