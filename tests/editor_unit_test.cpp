#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "editor.h"

namespace fs = std::filesystem;

// Test fixture for Editor::put_line()
class EditorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        editor = std::make_unique<Editor>();
        // Manually initialize editor state needed for tests
        editor->wksp     = std::make_unique<Workspace>(editor->tempfile_);
        editor->alt_wksp = std::make_unique<Workspace>(editor->tempfile_);

        // Open shared temp file
        editor->tempfile_.open_temp_file();

        //editor->current_line_no       = -1;
        //editor->current_line_modified = false;
        //editor->current_line          = "";
    }

    void TearDown() override { editor.reset(); }

    std::unique_ptr<Editor> editor;

    // Initialize with a few blank lines so put_line() can update them.
    // put_line() only updates existing lines, doesn't create new ones.
    void CreateBlankLines(unsigned num_lines)
    {
        editor->wksp->set_nlines(num_lines);
        Segment *blank_segs = Workspace::create_blank_lines(num_lines);
        editor->wksp->set_chain(blank_segs);
        editor->wksp->set_cursegm(blank_segs);
        editor->wksp->set_segmline(0);
    }
};

TEST_F(EditorTest, PutLineCreatesSegments)
{
    // Initially workspace has 5 blank lines
    CreateBlankLines(5);
    EXPECT_TRUE(editor->wksp->has_segments());
    EXPECT_EQ(editor->wksp->nlines(), 5);

    // Replace the first blank line with actual content via put_line
    editor->current_line          = "First line";
    editor->current_line_no       = 0;
    editor->current_line_modified = true;

    editor->put_line();

    // Verify workspace still has segments
    EXPECT_TRUE(editor->wksp->has_segments());
    EXPECT_NE(editor->wksp->chain(), nullptr);
    EXPECT_EQ(editor->wksp->nlines(), 5);

    // Verify we can read the updated line back
    std::string read_line = editor->wksp->read_line_from_segment(0);
    EXPECT_EQ("First line", read_line);
}

TEST_F(EditorTest, PutLineMultipleLines)
{
    CreateBlankLines(5);

    // Add first line
    editor->current_line          = "Line 1";
    editor->current_line_no       = 0;
    editor->current_line_modified = true;
    editor->put_line();

    // Add second line
    editor->current_line          = "Line 2";
    editor->current_line_no       = 1;
    editor->current_line_modified = true;
    editor->put_line();

    // Add third line
    editor->current_line          = "Line 3";
    editor->current_line_no       = 2;
    editor->current_line_modified = true;
    editor->put_line();

    // Verify workspace has the lines (we started with 5 blank lines)
    EXPECT_TRUE(editor->wksp->has_segments());
    EXPECT_EQ(editor->wksp->nlines(), 5);

    // Verify we can read the updated lines
    EXPECT_EQ(editor->wksp->read_line_from_segment(0), "Line 1");
    EXPECT_EQ(editor->wksp->read_line_from_segment(1), "Line 2");
    EXPECT_EQ(editor->wksp->read_line_from_segment(2), "Line 3");
}

TEST_F(EditorTest, PutLineUpdatesExistingSegments)
{
    CreateBlankLines(5);

    // First create some initial segments by adding lines
    editor->current_line          = "Original line 1";
    editor->current_line_no       = 0;
    editor->current_line_modified = true;
    editor->put_line();

    editor->current_line          = "Original line 2";
    editor->current_line_no       = 1;
    editor->current_line_modified = true;
    editor->put_line();

    editor->current_line          = "Original line 3";
    editor->current_line_no       = 2;
    editor->current_line_modified = true;
    editor->put_line();

    // Verify initial state (we started with 5 blank lines)
    EXPECT_EQ(editor->wksp->nlines(), 5);
    EXPECT_EQ(editor->wksp->read_line_from_segment(0), "Original line 1");
    EXPECT_EQ(editor->wksp->read_line_from_segment(1), "Original line 2");
    EXPECT_EQ(editor->wksp->read_line_from_segment(2), "Original line 3");

    // Now update line 1 (index 1) with new content
    editor->current_line          = "Updated line 2";
    editor->current_line_no       = 1;
    editor->current_line_modified = true;
    editor->put_line();

    // Verify the update (still 5 lines total)
    EXPECT_EQ(editor->wksp->nlines(), 5);
    EXPECT_EQ(editor->wksp->read_line_from_segment(0), "Original line 1");
    EXPECT_EQ(editor->wksp->read_line_from_segment(1), "Updated line 2");
    EXPECT_EQ(editor->wksp->read_line_from_segment(2), "Original line 3");

    // Update line 0 as well
    editor->current_line          = "Updated line 1";
    editor->current_line_no       = 0;
    editor->current_line_modified = true;
    editor->put_line();

    EXPECT_EQ(editor->wksp->read_line_from_segment(0), "Updated line 1");
    EXPECT_EQ(editor->wksp->read_line_from_segment(1), "Updated line 2");
    EXPECT_EQ(editor->wksp->read_line_from_segment(2), "Original line 3");
}

TEST_F(EditorTest, PutLineSegmentsPreserveContent)
{
    CreateBlankLines(5);

    // We started with 5 blank lines, update all of them
    const int num_lines = 5;
    for (int i = 0; i < num_lines; ++i) {
        editor->current_line          = "Line " + std::to_string(i);
        editor->current_line_no       = i;
        editor->current_line_modified = true;
        editor->put_line();
    }

    EXPECT_EQ(editor->wksp->nlines(), num_lines);

    // Verify all lines are readable
    for (int i = 0; i < num_lines; ++i) {
        std::string expected = "Line " + std::to_string(i);
        std::string actual   = editor->wksp->read_line_from_segment(i);
        EXPECT_EQ(expected, actual);
    }

    // Check segment chain structure
    Segment *seg = editor->wksp->chain();
    ASSERT_NE(seg, nullptr);

    // Collect segment info
    std::vector<Segment *> segments;
    Segment *curr = seg;
    while (curr) {
        if (curr->nlines > 0) {
            segments.push_back(curr);
        }
        curr = curr->next;
    }

    // Verify segments
    int total_lines = 0;
    for (auto *s : segments) {
        total_lines += s->nlines;
        EXPECT_GT(s->nlines, 0);
        EXPECT_GT(s->fdesc, 0); // Should have valid file descriptor
    }

    EXPECT_EQ(total_lines, num_lines);
}
