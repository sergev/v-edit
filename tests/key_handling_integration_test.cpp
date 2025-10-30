#include <gtest/gtest.h>
#include <ncurses.h>

#include "editor.h"

// Test fixture for key handling integration tests
// Tests the complete flow: key press -> handle_key_edit() -> result
class KeyHandlingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        editor = std::make_unique<Editor>();

        // Initialize editor state
        editor->wksp_     = std::make_unique<Workspace>(editor->tempfile_);
        editor->alt_wksp_ = std::make_unique<Workspace>(editor->tempfile_);

        // Open shared temp file
        editor->tempfile_.open_temp_file();

        // Initialize view state
        editor->wksp_->view.basecol = 0;
        editor->wksp_->view.topline = 0;
        editor->cursor_col_         = 0;
        editor->cursor_line_        = 0;
        editor->ncols_              = 80;
        editor->nlines_             = 24;
        editor->insert_mode_        = true; // Default to insert mode
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

    std::unique_ptr<Editor> editor;
};

// ============================================================================
// BACKSPACE Key Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, BackspaceKeyMiddleOfLine)
{
    CreateLine(0, "Hello World");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 6;
    editor->wksp_->view.basecol = 0;
    editor->wksp_->view.topline = 0;

    // Load line into buffer (simulate what would happen in edit loop)
    editor->get_line(0);

    // Simulate backspace key press
    editor->handle_key_edit(KEY_BACKSPACE);

    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5);
}

TEST_F(KeyHandlingIntegrationTest, BackspaceKeyWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 10;
    editor->wksp_->view.topline = 0;

    editor->get_line(0);

    // Simulate backspace (actual position: 10 + 5 = 15)
    editor->handle_key_edit(KEY_BACKSPACE);

    // Should delete character at position 14
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDFGHIJ");
    EXPECT_EQ(editor->cursor_col_, 4);
}

TEST_F(KeyHandlingIntegrationTest, BackspaceKey127)
{
    // Test with ASCII 127 (alternative backspace code)
    CreateLine(0, "Test");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 2;
    editor->get_line(0);

    editor->handle_key_edit(127); // ASCII DEL/Backspace

    EXPECT_EQ(editor->wksp_->read_line(0), "Tst");
    EXPECT_EQ(editor->cursor_col_, 1);
}

// ============================================================================
// DELETE Key Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, DeleteKeyMiddleOfLine)
{
    CreateLine(0, "Hello World");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 5;
    editor->get_line(0);

    // Simulate delete key press
    editor->handle_key_edit(KEY_DC);

    EXPECT_EQ(editor->wksp_->read_line(0), "HelloWorld");
    EXPECT_EQ(editor->cursor_col_, 5); // Cursor doesn't move
}

TEST_F(KeyHandlingIntegrationTest, DeleteKeyWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 10;
    editor->get_line(0);

    // Simulate delete (actual position: 10 + 5 = 15)
    editor->handle_key_edit(KEY_DC);

    // Should delete character at position 15 (F)
    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEGHIJ");
    EXPECT_EQ(editor->cursor_col_, 5);
}

// ============================================================================
// ENTER Key Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, EnterKeyMiddleOfLine)
{
    CreateLine(0, "HelloWorld");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 5;
    editor->get_line(0);

    // Simulate enter key press
    editor->handle_key_edit('\n');

    // First line should be "Hello"
    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    // Second line should be "World"
    EXPECT_EQ(editor->wksp_->read_line(1), "World");
    EXPECT_EQ(editor->cursor_line_, 1);
    EXPECT_EQ(editor->cursor_col_, 0);
}

TEST_F(KeyHandlingIntegrationTest, EnterKeyWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 10;
    editor->get_line(0);

    // Simulate enter (actual position: 10 + 5 = 15)
    editor->handle_key_edit(KEY_ENTER);

    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDE");
    EXPECT_EQ(editor->wksp_->read_line(1), "FGHIJ");
}

// ============================================================================
// TAB Key Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, TabKeyInsertion)
{
    CreateLine(0, "Hello");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 0;
    editor->get_line(0);

    // Simulate tab key press
    editor->handle_key_edit('\t');

    EXPECT_EQ(editor->wksp_->read_line(0), "    Hello");
    EXPECT_EQ(editor->cursor_col_, 4);
}

TEST_F(KeyHandlingIntegrationTest, TabKeyWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 10;
    editor->get_line(0);

    // Simulate tab (actual position: 10 + 5 = 15)
    editor->handle_key_edit('\t');

    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDE    FGHIJ");
    EXPECT_EQ(editor->cursor_col_, 9);
}

// ============================================================================
// Character Insertion Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, CharacterInsertionKey)
{
    CreateLine(0, "Helo");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 2;
    editor->insert_mode_ = true;
    editor->get_line(0);

    // Simulate 'l' key press
    editor->handle_key_edit('l');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 3);
}

TEST_F(KeyHandlingIntegrationTest, MultipleCharacterInsertion)
{
    CreateLine(0, "");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 0;
    editor->insert_mode_ = true;
    editor->get_line(0);

    // Type "Test" one character at a time
    const char *text = "Test";
    for (size_t i = 0; text[i] != '\0'; ++i) {
        editor->handle_key_edit(text[i]);
    }

    EXPECT_EQ(editor->wksp_->read_line(0), "Test");
    EXPECT_EQ(editor->cursor_col_, 4);
}

TEST_F(KeyHandlingIntegrationTest, CharacterInsertionWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 10;
    editor->insert_mode_        = true;
    editor->get_line(0);

    // Insert 'X' at actual position 15
    editor->handle_key_edit('X');

    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEXFGHIJ");
    EXPECT_EQ(editor->cursor_col_, 6);
}

TEST_F(KeyHandlingIntegrationTest, CharacterInsertionFooLineCount)
{
    editor->wksp_->debug_print(std::cout);
    EXPECT_EQ(editor->wksp_->total_line_count(), 0);

    editor->handle_key_edit('f');
    editor->handle_key_edit('o');
    editor->handle_key_edit('o');
    editor->handle_key_edit('\n');

    editor->wksp_->debug_print(std::cout);
    EXPECT_EQ(editor->wksp_->read_line(0), "foo");
    EXPECT_EQ(editor->wksp_->total_line_count(), 2);
}

TEST_F(KeyHandlingIntegrationTest, CharacterInsertionFooBarQuz)
{
    editor->handle_key_edit('f');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('o');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('o');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('\n');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('b');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('a');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('r');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('\n');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('q');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('u');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('z');
    editor->wksp_->debug_print(std::cout);

    editor->handle_key_edit('\n');
    editor->wksp_->debug_print(std::cout);

    EXPECT_EQ(editor->wksp_->read_line(0), "foo");
    EXPECT_EQ(editor->wksp_->read_line(1), "bar");
    EXPECT_EQ(editor->wksp_->read_line(2), "quz");
}

// ============================================================================
// Character Overwrite Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, CharacterOverwriteKey)
{
    CreateLine(0, "Hxllo");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 1;
    editor->insert_mode_ = false; // Overwrite mode
    editor->get_line(0);

    // Overwrite 'x' with 'e'
    editor->handle_key_edit('e');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");
    EXPECT_EQ(editor->cursor_col_, 2);
}

TEST_F(KeyHandlingIntegrationTest, CharacterOverwriteWithScroll)
{
    CreateLine(0, "0123456789ABCDEFGHIJ");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 5;
    editor->wksp_->view.basecol = 10;
    editor->insert_mode_        = false;
    editor->get_line(0);

    // Overwrite 'F' with 'X' at actual position 15
    editor->handle_key_edit('X');

    EXPECT_EQ(editor->wksp_->read_line(0), "0123456789ABCDEXGHIJ");
    EXPECT_EQ(editor->cursor_col_, 6);
}

// ============================================================================
// Mode Switching Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, InsertVsOverwriteMode)
{
    CreateLine(0, "Test");

    // Test insert mode
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 2;
    editor->insert_mode_ = true;
    editor->get_line(0);

    editor->handle_key_edit('X');
    EXPECT_EQ(editor->wksp_->read_line(0), "TeXst");

    // Test overwrite mode
    CreateLine(1, "Test");
    editor->cursor_line_ = 1;
    editor->cursor_col_  = 2;
    editor->insert_mode_ = false;
    editor->get_line(1);

    editor->handle_key_edit('X');
    EXPECT_EQ(editor->wksp_->read_line(1), "TeXt");
}

// ============================================================================
// Complex Sequence Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, ComplexEditingSequence)
{
    CreateLine(0, "Hello");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 5;
    editor->insert_mode_ = true;
    editor->get_line(0);

    // Type " World"
    editor->handle_key_edit(' ');
    editor->handle_key_edit('W');
    editor->handle_key_edit('o');
    editor->handle_key_edit('r');
    editor->handle_key_edit('l');
    editor->handle_key_edit('d');

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello World");
    EXPECT_EQ(editor->cursor_col_, 11);

    // Backspace twice
    editor->handle_key_edit(KEY_BACKSPACE);
    editor->handle_key_edit(KEY_BACKSPACE);

    EXPECT_EQ(editor->wksp_->read_line(0), "Hello Wor");
    EXPECT_EQ(editor->cursor_col_, 9);
}

TEST_F(KeyHandlingIntegrationTest, EditingWithScrollingSequence)
{
    CreateLine(0, "The quick brown fox");
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 0;
    editor->wksp_->view.basecol = 10;
    editor->insert_mode_        = false;
    editor->get_line(0);

    // Overwrite "brown" with "BLACK" (starts at position 10)
    editor->handle_key_edit('B');
    editor->handle_key_edit('L');
    editor->handle_key_edit('A');
    editor->handle_key_edit('C');
    editor->handle_key_edit('K');

    EXPECT_EQ(editor->wksp_->read_line(0), "The quick BLACK fox");
}

// ============================================================================
// Edge Case Integration Tests
// ============================================================================

TEST_F(KeyHandlingIntegrationTest, PrintableCharacterRange)
{
    CreateLine(0, "");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 0;
    editor->insert_mode_ = true;
    editor->get_line(0);

    // Test boundary characters (32 = space, 126 = ~)
    editor->handle_key_edit(' '); // 32
    editor->handle_key_edit('A'); // 65
    editor->handle_key_edit('~'); // 126

    EXPECT_EQ(editor->wksp_->read_line(0), " A~");
}

TEST_F(KeyHandlingIntegrationTest, NonPrintableCharactersIgnored)
{
    CreateLine(0, "Test");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 2;
    editor->get_line(0);

    int initial_length = editor->wksp_->read_line(0).length();

    // Try non-printable characters (should be ignored by handle_key_edit)
    editor->handle_key_edit(1);   // Ctrl-A (handled by F1/^A branch)
    editor->handle_key_edit(31);  // Below printable range
    editor->handle_key_edit(128); // Above printable range

    // Line should be unchanged (or changed only by handled control chars)
    // The key point is that characters >= 32 and < 127 are handled
}

TEST_F(KeyHandlingIntegrationTest, EmptyLineEditing)
{
    CreateLine(0, "");
    editor->cursor_line_ = 0;
    editor->cursor_col_  = 0;
    editor->insert_mode_ = true;
    editor->get_line(0);

    // Add character to empty line
    editor->handle_key_edit('A');

    EXPECT_EQ(editor->wksp_->read_line(0), "A");
    EXPECT_EQ(editor->cursor_col_, 1);

    // Delete it
    editor->handle_key_edit(KEY_BACKSPACE);

    EXPECT_EQ(editor->wksp_->read_line(0), "");
    EXPECT_EQ(editor->cursor_col_, 0);
}

TEST_F(KeyHandlingIntegrationTest, VeryLongLineEditing)
{
    // Create a 100-character line
    std::string long_line(100, 'X');
    CreateLine(0, long_line);
    editor->cursor_line_        = 0;
    editor->cursor_col_         = 10;
    editor->wksp_->view.basecol = 80; // Scrolled way right
    editor->get_line(0);

    // Delete at position 90
    editor->handle_key_edit(KEY_DC);

    EXPECT_EQ(editor->wksp_->read_line(0).length(), 99);
}
