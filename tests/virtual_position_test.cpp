#include <gtest/gtest.h>

#include "editor.h"

// Test fixture for virtual position typing
class VirtualPositionTest : public ::testing::Test {
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
        editor->insert_mode_        = true; // Use insert mode
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

    std::unique_ptr<Editor> editor;
};

// Test typing beyond line contents (virtual column position)
TEST_F(VirtualPositionTest, TypeBeyondLineContents)
{
    // Create a line with some content
    CreateLine(0, "Hello");
    EXPECT_EQ(editor->wksp_->total_line_count(), 1);
    EXPECT_EQ(editor->wksp_->read_line(0), "Hello");

    // Position cursor beyond the end of the line (virtual column position)
    // Line is 5 characters, position cursor at column 15
    editor->wksp_->view.topline = 0;
    editor->cursor_line_        = 0;
    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 15; // Beyond "Hello" (which is 5 chars)

    // Verify we're at a virtual position
    EXPECT_EQ(editor->wksp_->read_line(0).size(), 5);
    EXPECT_EQ(editor->get_actual_col(), 15);

    // Type text at virtual column position
    std::string text_to_type = "World";
    for (char ch : text_to_type) {
        editor->edit_insert_char(ch);
    }

    // Verify the result: should pad with spaces then insert
    std::string result = editor->wksp_->read_line(0);
    EXPECT_EQ(result.size(), 20); // 5 (Hello) + 10 (spaces) + 5 (World)
    EXPECT_EQ(result, "Hello          World");
}

// Test typing beyond file end (virtual line position)
TEST_F(VirtualPositionTest, TypeBeyondFileEnd)
{
    // Create a file with one line
    CreateLine(0, "First line");
    EXPECT_EQ(editor->wksp_->total_line_count(), 1);

    // Position cursor 3 lines beyond the end of the file
    // File has 1 line (index 0), position at line 3 (4th line)
    editor->wksp_->view.topline = 3;
    editor->cursor_line_        = 0;
    editor->wksp_->view.basecol = 0;
    editor->cursor_col_         = 0;

    // Verify we're at a virtual line position
    EXPECT_EQ(editor->wksp_->total_line_count(), 1);
    int abs_line = editor->wksp_->view.topline + editor->cursor_line_;
    EXPECT_EQ(abs_line, 3); // Beyond file end

    // Type text at virtual line position
    std::string text_to_type = "Virtual line";
    for (char ch : text_to_type) {
        editor->edit_insert_char(ch);
    }

    // Verify new lines were created with proper content
    // When typing at line 3, it should create blank lines for 1, 2, and content at 3
    EXPECT_EQ(editor->wksp_->total_line_count(), 4); // At least 4 lines (0, 1, 2, 3)

    // Verify contents
    EXPECT_EQ(editor->wksp_->read_line(0), "First line");
    EXPECT_EQ(editor->wksp_->read_line(1), "");
    EXPECT_EQ(editor->wksp_->read_line(2), "");
    EXPECT_EQ(editor->wksp_->read_line(3), "Virtual line");
}

// Test typing at virtual column position beyond line end with horizontal scroll
TEST_F(VirtualPositionTest, TypeBeyondLineContentsWithScroll)
{
    // Create a long line
    std::string long_line = "This is a longer line with some content";
    CreateLine(0, long_line);
    EXPECT_EQ(editor->wksp_->read_line(0), long_line);

    // Position cursor far beyond the end using basecol (horizontal scrolling)
    editor->wksp_->view.topline = 0;
    editor->cursor_line_        = 0;
    editor->wksp_->view.basecol = 100; // Scroll to column 100
    editor->cursor_col_         = 10;  // Additional 10 columns

    // Verify we're at a virtual position
    EXPECT_EQ(editor->wksp_->read_line(0).size(), (int)long_line.size());
    EXPECT_EQ(editor->get_actual_col(), 110); // basecol + cursor_col_

    // Type text at virtual column position
    std::string text_to_type = "EXTRA";
    for (char ch : text_to_type) {
        editor->edit_insert_char(ch);
    }

    // Verify the result: should pad with spaces then insert
    std::string result = editor->wksp_->read_line(0);
    EXPECT_GE(result.size(), 110 + 5); // Original + padding + new text
    EXPECT_TRUE(result.find("EXTRA") != std::string::npos);
}
