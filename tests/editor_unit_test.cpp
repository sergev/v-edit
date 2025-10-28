#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <fstream>

#include "editor.h"

// Test fixture for Editor::put_line()
class EditorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        editor = std::make_unique<Editor>();
        // Manually initialize editor state needed for tests
        editor->wksp_     = std::make_unique<Workspace>(editor->tempfile_);
        editor->alt_wksp_ = std::make_unique<Workspace>(editor->tempfile_);

        // Open shared temp file
        editor->tempfile_.open_temp_file();

        // editor->current_line_no_       = -1;
        // editor->current_line_modified_ = false;
        // editor->current_line_          = "";
    }

    void TearDown() override { editor.reset(); }

    std::unique_ptr<Editor> editor;

    // Initialize with a few blank lines so put_line() can update them.
    // put_line() only updates existing lines, doesn't create new ones.
    void CreateBlankLines(unsigned num_lines)
    {
        editor->wksp_->set_nlines(num_lines);
        std::list<Segment> blank_segs = Workspace::create_blank_lines(num_lines);
        editor->wksp_->set_chain(blank_segs);
        editor->wksp_->set_cursegm(editor->wksp_->get_segments().begin());
        editor->wksp_->set_segmline(0);
    }
};

TEST_F(EditorTest, PutLineCreatesSegments)
{
    // Initially workspace has 5 blank lines
    CreateBlankLines(5);
    EXPECT_TRUE(editor->wksp_->has_segments());
    EXPECT_EQ(editor->wksp_->nlines(), 5);

    // Replace the first blank line with actual content via put_line
    editor->current_line_          = "First line";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;

    editor->put_line();

    // Verify workspace still has segments
    EXPECT_TRUE(editor->wksp_->has_segments());
    EXPECT_TRUE(editor->wksp_->has_segments());
    EXPECT_EQ(editor->wksp_->nlines(), 5);

    // Verify we can read the updated line back
    std::string read_line = editor->wksp_->read_line_from_segment(0);
    EXPECT_EQ("First line", read_line);
}

TEST_F(EditorTest, PutLineMultipleLines)
{
    CreateBlankLines(5);

    // Add first line
    editor->current_line_          = "Line 1";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Add second line
    editor->current_line_          = "Line 2";
    editor->current_line_no_       = 1;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Add third line
    editor->current_line_          = "Line 3";
    editor->current_line_no_       = 2;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Verify workspace has the lines (we started with 5 blank lines)
    EXPECT_TRUE(editor->wksp_->has_segments());
    EXPECT_EQ(editor->wksp_->nlines(), 5);

    // Verify we can read the updated lines
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Line 1");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(1), "Line 2");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(2), "Line 3");
}

TEST_F(EditorTest, PutLineUpdatesExistingSegments)
{
    CreateBlankLines(5);

    // First create some initial segments by adding lines
    editor->current_line_          = "Original line 1";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    editor->current_line_          = "Original line 2";
    editor->current_line_no_       = 1;
    editor->current_line_modified_ = true;
    editor->put_line();

    editor->current_line_          = "Original line 3";
    editor->current_line_no_       = 2;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Verify initial state (we started with 5 blank lines)
    EXPECT_EQ(editor->wksp_->nlines(), 5);
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Original line 1");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(1), "Original line 2");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(2), "Original line 3");

    // Now update line 1 (index 1) with new content
    editor->current_line_          = "Updated line 2";
    editor->current_line_no_       = 1;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Verify the update (still 5 lines total)
    EXPECT_EQ(editor->wksp_->nlines(), 5);
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Original line 1");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(1), "Updated line 2");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(2), "Original line 3");

    // Update line 0 as well
    editor->current_line_          = "Updated line 1";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Updated line 1");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(1), "Updated line 2");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(2), "Original line 3");
}

TEST_F(EditorTest, PutLineSegmentsPreserveContent)
{
    CreateBlankLines(5);

    // We started with 5 blank lines, update all of them
    const int num_lines = 5;
    for (int i = 0; i < num_lines; ++i) {
        editor->current_line_          = "Line " + std::to_string(i);
        editor->current_line_no_       = i;
        editor->current_line_modified_ = true;
        editor->put_line();
    }

    EXPECT_EQ(editor->wksp_->nlines(), num_lines);

    // Verify all lines are readable
    for (int i = 0; i < num_lines; ++i) {
        std::string expected = "Line " + std::to_string(i);
        std::string actual   = editor->wksp_->read_line_from_segment(i);
        EXPECT_EQ(expected, actual);
    }

    // Check segment chain structure - iterate through std::list
    const auto &segments_list = editor->wksp_->get_segments();
    std::vector<const Segment *> segments;
    for (const auto &s : segments_list) {
        if (s.nlines > 0) {
            segments.push_back(&s);
        }
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

TEST_F(EditorTest, PutLineCreatesFirstLineFromEmptyWorkspace)
{
    // Initially workspace is empty (has tail segment)
    EXPECT_TRUE(editor->wksp_->has_segments());
    EXPECT_EQ(editor->wksp_->nlines(), 0);

    // Verify the tail segment
    auto tail_it = editor->wksp_->get_segments().begin();
    ASSERT_NE(tail_it, editor->wksp_->get_segments().end());
    EXPECT_EQ(tail_it->nlines, 0);
    EXPECT_EQ(tail_it->fdesc, 0);

    // Add the first line via put_line (should create it, not fail)
    editor->current_line_          = "First line";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;

    editor->put_line();

    // Verify workspace now has the line
    EXPECT_TRUE(editor->wksp_->has_segments());
    EXPECT_EQ(editor->wksp_->nlines(), 1);

    // Verify we can read the line back
    std::string read_line = editor->wksp_->read_line_from_segment(0);
    EXPECT_EQ("First line", read_line);
}

TEST_F(EditorTest, PutLineExtendsFileBeyondEnd)
{
    // Add first line
    editor->current_line_          = "Line 1";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Verify first line exists
    EXPECT_EQ(editor->wksp_->nlines(), 1);
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Line 1");

    // Add third line (skipping line 1) - should create blank line in between
    editor->current_line_          = "Line 3";
    editor->current_line_no_       = 2;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Verify file was extended
    EXPECT_EQ(editor->wksp_->nlines(), 3);
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Line 1");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(1), ""); // Created blank line
    EXPECT_EQ(editor->wksp_->read_line_from_segment(2), "Line 3");
}

TEST_F(EditorTest, PutLineCreatesMultipleLinesSequentially)
{
    // Add multiple lines sequentially
    for (int i = 0; i < 5; ++i) {
        editor->current_line_          = "Line " + std::to_string(i);
        editor->current_line_no_       = i;
        editor->current_line_modified_ = true;
        editor->put_line();
    }

    // Verify all lines exist
    EXPECT_EQ(editor->wksp_->nlines(), 5);

    for (int i = 0; i < 5; ++i) {
        std::string expected = "Line " + std::to_string(i);
        std::string actual   = editor->wksp_->read_line_from_segment(i);
        EXPECT_EQ(expected, actual);
    }
}

TEST_F(EditorTest, PutLineUpdatesExistingLine)
{
    // Create first line
    editor->current_line_          = "Original";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Original");

    // Update the line
    editor->current_line_          = "Updated";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->nlines(), 1);
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Updated");
}

TEST_F(EditorTest, PutLineWithGapsCreatesBlankLines)
{
    // Add line 0
    editor->current_line_          = "Start";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    // Add line 10 (creates lines 1-9 as blank)
    editor->current_line_          = "End";
    editor->current_line_no_       = 10;
    editor->current_line_modified_ = true;
    editor->put_line();

    EXPECT_EQ(editor->wksp_->nlines(), 11);
    EXPECT_EQ(editor->wksp_->read_line_from_segment(0), "Start");
    EXPECT_EQ(editor->wksp_->read_line_from_segment(10), "End");

    // Lines 1-9 should be blank
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(editor->wksp_->read_line_from_segment(i), "");
    }
}

TEST_F(EditorTest, PutLineSegmentChainIntegrity)
{
    // Add several lines
    for (int i = 0; i < 10; ++i) {
        editor->current_line_          = "Item " + std::to_string(i);
        editor->current_line_no_       = i;
        editor->current_line_modified_ = true;
        editor->put_line();
    }

    // Verify segment chain structure - iterate through std::list
    const auto &segments_list = editor->wksp_->get_segments();
    std::vector<const Segment *> segments;
    for (const auto &s : segments_list) {
        if (s.nlines > 0) {
            segments.push_back(&s);
        }
    }

    // Should have at least one segment
    EXPECT_GT(segments.size(), 0);

    // Verify all segments have valid file descriptors (not tail)
    for (auto *s : segments) {
        EXPECT_GT(s->fdesc, 0);
        EXPECT_GT(s->nlines, 0);
    }

    // Verify total lines across segments
    int total_lines = 0;
    for (auto *s : segments) {
        total_lines += s->nlines;
    }
    EXPECT_EQ(total_lines, 10);
}

// Unit tests for Workspace::position() method
TEST_F(EditorTest, PositionValidLine)
{
    // Create workspace with 5 lines
    CreateBlankLines(5);

    // Set current segment to valid line
    EXPECT_EQ(editor->wksp_->set_current_segment(2), 0);
    EXPECT_EQ(editor->wksp_->line(), 2);
}

TEST_F(EditorTest, SetCurrentSegmentFirstLine)
{
    CreateBlankLines(5);

    // Set current segment at first line
    EXPECT_EQ(editor->wksp_->set_current_segment(0), 0);
    EXPECT_EQ(editor->wksp_->line(), 0);
    EXPECT_EQ(editor->wksp_->cursegm(), editor->wksp_->get_segments().begin());
}

TEST_F(EditorTest, SetCurrentSegmentLastLine)
{
    CreateBlankLines(5);

    // Set current segment at last line (line 4)
    EXPECT_EQ(editor->wksp_->set_current_segment(4), 0);
    EXPECT_EQ(editor->wksp_->line(), 4);
}

TEST_F(EditorTest, SetCurrentSegmentMidFile)
{
    CreateBlankLines(100);

    // Set current segment in middle of file
    EXPECT_EQ(editor->wksp_->set_current_segment(50), 0);
    EXPECT_EQ(editor->wksp_->line(), 50);
}

TEST_F(EditorTest, SetCurrentSegmentNegativeLineThrows)
{
    CreateBlankLines(5);

    // Negative line number should throw
    EXPECT_THROW(editor->wksp_->set_current_segment(-1), std::runtime_error);
}

TEST_F(EditorTest, SetCurrentSegmentBeyondEndReturnsOne)
{
    CreateBlankLines(5);

    // Line beyond end should return 1 (not throw)
    EXPECT_EQ(editor->wksp_->set_current_segment(10), 1);
}

TEST_F(EditorTest, SetCurrentSegmentBeyondEndByOne)
{
    CreateBlankLines(5);

    // Try to set current segment at line 5 (one past end) should return 1
    EXPECT_EQ(editor->wksp_->set_current_segment(5), 1);
}

TEST_F(EditorTest, SetCurrentSegmentEmptyWorkspaceReturnsOne)
{
    // Empty workspace (has tail but no content)
    EXPECT_EQ(editor->wksp_->nlines(), 0);

    // Any position should return 1 (beyond end)
    EXPECT_EQ(editor->wksp_->set_current_segment(0), 1);
    EXPECT_EQ(editor->wksp_->set_current_segment(1), 1);
}

TEST_F(EditorTest, SetCurrentSegmentUpdatesCursegm)
{
    CreateBlankLines(20);

    // Set current segment at line 5
    EXPECT_EQ(editor->wksp_->set_current_segment(5), 0);
    auto seg5_it = editor->wksp_->cursegm();

    // Set current segment at line 10
    EXPECT_EQ(editor->wksp_->set_current_segment(10), 0);
    auto seg10_it = editor->wksp_->cursegm();

    // Should potentially be different segments
    EXPECT_EQ(editor->wksp_->line(), 10);

    // Set current segment back to line 5
    EXPECT_EQ(editor->wksp_->set_current_segment(5), 0);
    EXPECT_EQ(editor->wksp_->line(), 5);
}

TEST_F(EditorTest, SetCurrentSegmentUpdatesSegmline)
{
    CreateBlankLines(20);

    // Set current segment at line 0
    editor->wksp_->set_current_segment(0);
    EXPECT_EQ(editor->wksp_->segmline(), 0);

    // Set current segment at line 10
    editor->wksp_->set_current_segment(10);
    int segmline10 = editor->wksp_->segmline();
    EXPECT_GE(segmline10, 0);
    EXPECT_LE(segmline10, 10);

    // Set current segment at line 15
    editor->wksp_->set_current_segment(15);
    int segmline15 = editor->wksp_->segmline();
    EXPECT_GE(segmline15, segmline10);
    EXPECT_LE(segmline15, 15);
}

TEST_F(EditorTest, SetCurrentSegmentBackwardMovement)
{
    CreateBlankLines(20);

    // Start at end
    editor->wksp_->set_current_segment(19);
    auto endSeg_it = editor->wksp_->cursegm();

    // Move backward to beginning
    editor->wksp_->set_current_segment(0);
    auto startSeg_it = editor->wksp_->cursegm();

    EXPECT_EQ(editor->wksp_->line(), 0);
    EXPECT_EQ(editor->wksp_->segmline(), 0);
}

TEST_F(EditorTest, SetCurrentSegmentForwardMovement)
{
    CreateBlankLines(20);

    // Start at beginning
    editor->wksp_->set_current_segment(0);

    // Move forward to end
    editor->wksp_->set_current_segment(19);

    EXPECT_EQ(editor->wksp_->line(), 19);
}

TEST_F(EditorTest, SetCurrentSegmentBoundaryAtZero)
{
    CreateBlankLines(10);

    // Set current segment at 0
    EXPECT_EQ(editor->wksp_->set_current_segment(0), 0);
    EXPECT_EQ(editor->wksp_->line(), 0);

    // Set current segment at 1
    EXPECT_EQ(editor->wksp_->set_current_segment(1), 0);
    EXPECT_EQ(editor->wksp_->line(), 1);
}

TEST_F(EditorTest, SetCurrentSegmentBoundaryAtEnd)
{
    CreateBlankLines(10);

    // Set current segment at last line (9)
    EXPECT_EQ(editor->wksp_->set_current_segment(9), 0);
    EXPECT_EQ(editor->wksp_->line(), 9);

    // Try to set current segment beyond (should return 1, not throw)
    EXPECT_EQ(editor->wksp_->set_current_segment(10), 1);
}

TEST_F(EditorTest, SetCurrentSegmentLargeFile)
{
    CreateBlankLines(1000);

    // Set current segment at various points
    EXPECT_EQ(editor->wksp_->set_current_segment(0), 0);
    EXPECT_EQ(editor->wksp_->set_current_segment(500), 0);
    EXPECT_EQ(editor->wksp_->set_current_segment(999), 0);

    // Beyond end (should return 1, not throw)
    EXPECT_EQ(editor->wksp_->set_current_segment(1000), 1);
}

TEST_F(EditorTest, SetCurrentSegmentConsistency)
{
    CreateBlankLines(50);

    // Set current segment multiple times at same line
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(editor->wksp_->set_current_segment(25), 0);
        EXPECT_EQ(editor->wksp_->line(), 25);
    }
}

TEST_F(EditorTest, SetCurrentSegmentSequenceForward)
{
    CreateBlankLines(30);

    // Set current segment sequentially forward
    for (int i = 0; i < 30; i++) {
        EXPECT_EQ(editor->wksp_->set_current_segment(i), 0);
        EXPECT_EQ(editor->wksp_->line(), i);
    }
}

TEST_F(EditorTest, SetCurrentSegmentSequenceBackward)
{
    CreateBlankLines(30);

    // Set current segment sequentially backward
    for (int i = 29; i >= 0; i--) {
        EXPECT_EQ(editor->wksp_->set_current_segment(i), 0);
        EXPECT_EQ(editor->wksp_->line(), i);
    }
}

TEST_F(EditorTest, SetCurrentSegmentRandomAccess)
{
    CreateBlankLines(50);

    // Random access pattern
    std::vector<int> random_lines = { 0, 25, 49, 12, 33, 7, 40, 3, 30, 15 };
    for (int line : random_lines) {
        EXPECT_EQ(editor->wksp_->set_current_segment(line), 0);
        EXPECT_EQ(editor->wksp_->line(), line);
    }
}

TEST_F(EditorTest, DebugPutLineGaps)
{
    std::cout << "\n=== Test: Put line at 0 ===\n";
    // Add first line
    editor->current_line_          = "Line 1";
    editor->current_line_no_       = 0;
    editor->current_line_modified_ = true;
    editor->put_line();

    std::cout << "After put_line(0):\n";
    editor->wksp_->debug_print(std::cout);

    // Verify we can read it
    std::string line0 = editor->wksp_->read_line_from_segment(0);
    std::cout << "Line 0: '" << line0 << "'\n";

    std::cout << "\n=== Test: Put line at 2 ===\n";
    // Add third line (skipping line 1)
    editor->current_line_          = "Line 3";
    editor->current_line_no_       = 2;
    editor->current_line_modified_ = true;
    editor->put_line();

    std::cout << "After put_line(2):\n";
    editor->wksp_->debug_print(std::cout);

    // Verify all three lines exist
    EXPECT_EQ(editor->wksp_->nlines(), 3);
    line0             = editor->wksp_->read_line_from_segment(0);
    std::string line1 = editor->wksp_->read_line_from_segment(1);
    std::string line2 = editor->wksp_->read_line_from_segment(2);

    std::cout << "Line 0: '" << line0 << "'\n";
    std::cout << "Line 1: '" << line1 << "'\n";
    std::cout << "Line 2: '" << line2 << "'\n";

    EXPECT_EQ(line0, "Line 1");
    EXPECT_EQ(line1, ""); // Should be blank
    EXPECT_EQ(line2, "Line 3");
}
