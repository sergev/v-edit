#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "tempfile.h"
#include "workspace.h"

namespace fs = std::filesystem;

// Workspace test fixture
class WorkspaceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        tempfile = std::make_unique<Tempfile>();
        wksp     = std::make_unique<Workspace>(*tempfile);
        // Open temp file for workspace use
        tempfile->open_temp_file();
    }

    void TearDown() override
    {
        wksp.reset();
        tempfile.reset();
    }

    std::unique_ptr<Tempfile> tempfile;
    std::unique_ptr<Workspace> wksp;

    std::string createTestFile(const std::string &content)
    {
        const std::string testName =
            ::testing::UnitTest::GetInstance()->current_test_info()->name();
        const std::string filename = testName + ".txt";
        std::ofstream f(filename);
        f << content;
        f.close();
        return filename;
    }

    void cleanupTestFile(const std::string &filename) { fs::remove(filename); }

    Workspace *getWksp() { return wksp.get(); }
};

//
// Test create_blank_lines
//
TEST_F(WorkspaceTest, CreateBlankLines)
{
    Segment *seg = Workspace::create_blank_lines(5);

    EXPECT_NE(seg, nullptr);
    EXPECT_EQ(seg->nlines, 5);
    EXPECT_EQ(seg->fdesc, -1);

    // Check data
    EXPECT_EQ(seg->sizes.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(seg->sizes[i], 1);
    }

    // Cleanup
    Workspace::cleanup_segments(seg);
}

TEST_F(WorkspaceTest, CreateBlankLinesLarge)
{
    // Test with > 127 lines to verify segment splitting
    Segment *seg = Workspace::create_blank_lines(200);

    EXPECT_NE(seg, nullptr);

    // Should have at least 2 segments (127 + 73)
    int seg_count   = 0;
    int total_lines = 0;
    Segment *curr   = seg;

    while (curr && curr->fdesc != 0) {
        if (curr->nlines > 0) {
            total_lines += curr->nlines;
            seg_count++;
        }
        curr = curr->next;
    }

    EXPECT_EQ(total_lines, 200);
    EXPECT_GT(seg_count, 1);

    // Cleanup
    Workspace::cleanup_segments(seg);
}

TEST_F(WorkspaceTest, CleanupSegments)
{
    // Create a segment chain
    Segment *seg = Workspace::create_blank_lines(10);

    // Verify segments were created
    EXPECT_NE(seg, nullptr);

    int count     = 0;
    Segment *curr = seg;
    while (curr) {
        count++;
        curr = curr->next;
    }
    EXPECT_GT(count, 1);

    // Cleanup
    Workspace::cleanup_segments(seg);

    // Should not crash - cleanup handled
    EXPECT_TRUE(true);
}

//
// Test copy_segment_chain
//
TEST_F(WorkspaceTest, CopySegmentChain)
{
    // First create a test segment chain
    Segment *original = Workspace::create_blank_lines(10);

    Segment *copy = Workspace::copy_segment_chain(original, nullptr);

    EXPECT_NE(copy, nullptr);
    EXPECT_EQ(copy->nlines, original->nlines);

    // Verify it's a deep copy (different pointers but same data)
    EXPECT_NE(copy, original);
    EXPECT_EQ(copy->sizes.size(), original->sizes.size());

    for (size_t i = 0; i < copy->sizes.size(); ++i) {
        EXPECT_EQ(copy->sizes[i], original->sizes[i]);
    }

    // Cleanup
    Workspace::cleanup_segments(original);
    Workspace::cleanup_segments(copy);
}

TEST_F(WorkspaceTest, CopySegmentChainPartial)
{
    Segment *original = Workspace::create_blank_lines(20);

    // Copy only first 3 lines
    Segment *end_marker = original;
    for (int i = 0; i < 3; ++i) {
        if (end_marker && end_marker->next)
            end_marker = end_marker->next;
    }

    Segment *copy = Workspace::copy_segment_chain(original, end_marker);

    EXPECT_NE(copy, nullptr);

    // Cleanup
    Workspace::cleanup_segments(original);
    Workspace::cleanup_segments(copy);
}

//
// Test breaksegm
//
TEST_F(WorkspaceTest, BreakSegmentAtLine)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\nLine 3\nLine 4\n");

    wksp->load_file_to_segments(filename);

    // Break at line 2
    int result = wksp->breaksegm(2, true);

    EXPECT_EQ(result, 0);

    // Verify segmentation
    EXPECT_NE(wksp->cursegm(), nullptr);
    EXPECT_EQ(wksp->segmline(), 2);

    // Current segment should start at line 2
    Segment *seg = wksp->cursegm();
    if (seg) {
        // The segment should now contain lines from 2 onwards
        EXPECT_GE(seg->nlines, 0);
    }

    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, BreakSegmentAtFirstLine)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\n");

    wksp->load_file_to_segments(filename);

    // Break at line 0
    int result = wksp->breaksegm(0, true);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(wksp->segmline(), 0);

    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, BreakSegmentBeyondEnd)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\n");

    wksp->load_file_to_segments(filename);

    // Try to break at line 100 (beyond end)
    int result = wksp->breaksegm(100, true);

    // Should handle gracefully
    EXPECT_GE(result, 0); // Returns 0 if it creates empty segments, 1 if error

    cleanupTestFile(filename);
}

//
// Test catsegm
//
TEST_F(WorkspaceTest, CatSegmentMerge)
{
    std::string filename = createTestFile("Line 0\nLine 1\n");

    wksp->load_file_to_segments(filename);

    // catsegm should try to merge current with previous
    // Result depends on segment structure
    bool merged = wksp->catsegm();

    // Just verify no crash
    EXPECT_TRUE(true);

    cleanupTestFile(filename);
}

//
// Test insert_segments
//
TEST_F(WorkspaceTest, InsertSegments)
{
    std::string filename = createTestFile("Original line 1\nOriginal line 2\n");

    wksp->load_file_to_segments(filename);

    // Create blank lines to insert
    Segment *to_insert = Workspace::create_blank_lines(3);

    // Insert at line 1
    wksp->insert_segments(to_insert, 1);

    // Verify insertion
    EXPECT_EQ(wksp->line(), 1);

    // Cleanup
    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, InsertSegmentsAtBeginning)
{
    std::string filename = createTestFile("Line 0\nLine 1\n");

    wksp->load_file_to_segments(filename);

    Segment *to_insert = Workspace::create_blank_lines(2);
    wksp->insert_segments(to_insert, 0);

    EXPECT_TRUE(wksp->has_segments());

    cleanupTestFile(filename);
}

//
// Test delete_segments
//
TEST_F(WorkspaceTest, DeleteSegments)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\nLine 3\nLine 4\n");

    wksp->load_file_to_segments(filename);

    // Delete lines 1-3
    Segment *deleted = wksp->delete_segments(1, 3);

    // Should return the deleted chain
    EXPECT_NE(deleted, nullptr);

    // Cleanup deleted segments
    Workspace::cleanup_segments(deleted);

    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, DeleteSegmentsRange)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\nLine 3\n");

    wksp->load_file_to_segments(filename);

    // Delete single line
    Segment *deleted = wksp->delete_segments(1, 1);

    EXPECT_NE(deleted, nullptr);

    // Cleanup
    Workspace::cleanup_segments(deleted);
    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, DeleteSegmentsInvalidRange)
{
    std::string filename = createTestFile("Line 0\nLine 1\n");

    wksp->load_file_to_segments(filename);

    // Try to delete invalid range (from > to)
    Segment *deleted = wksp->delete_segments(5, 2);

    EXPECT_EQ(deleted, nullptr);

    cleanupTestFile(filename);
}

//
// Integration test for workflow
//
TEST_F(WorkspaceTest, InsertDeleteWorkflow)
{
    std::string filename = createTestFile("Original line 1\nOriginal line 2\nOriginal line 3\n");

    wksp->load_file_to_segments(filename);

    // Insert blank lines
    Segment *blank_lines = Workspace::create_blank_lines(2);
    wksp->insert_segments(blank_lines, 1);

    // Delete the inserted lines
    Segment *deleted = wksp->delete_segments(1, 2);

    EXPECT_NE(deleted, nullptr);

    // Cleanup
    Workspace::cleanup_segments(deleted);
    cleanupTestFile(filename);
}

//
// Test segment chain manipulation
//
TEST_F(WorkspaceTest, SegmentChainCopyAndInsert)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\n");

    wksp->load_file_to_segments(filename);

    // Copy part of the chain
    Segment *orig_chain = wksp->chain();
    Segment *copy       = Workspace::copy_segment_chain(orig_chain, nullptr);

    EXPECT_NE(copy, nullptr);
    EXPECT_NE(copy, orig_chain);

    // Insert the copy elsewhere
    wksp->insert_segments(copy, 0);

    // Cleanup
    cleanupTestFile(filename);
}

//
// Test with empty workspace
//
TEST_F(WorkspaceTest, EmptyWorkspaceOperations)
{
    // Try operations on empty workspace
    int result = wksp->breaksegm(0, true);
    EXPECT_NE(result, 0); // Should fail

    bool merged = wksp->catsegm();
    EXPECT_FALSE(merged);

    Segment *deleted = wksp->delete_segments(0, 1);
    EXPECT_EQ(deleted, nullptr);
}

//
// Test loading and operations
//
TEST_F(WorkspaceTest, LoadAndPosition)
{
    std::string filename = createTestFile("Line 0\nLine 1\nLine 2\nLine 3\nLine 4\n");

    wksp->load_file_to_segments(filename);

    // Position at line 3
    int result = wksp->position(3);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(wksp->line(), 3);

    // Break at line 3
    result = wksp->breaksegm(3, true);
    EXPECT_EQ(result, 0);

    cleanupTestFile(filename);
}

//
// Test large file with segments
//
TEST_F(WorkspaceTest, LargeFileSegmentOperations)
{
    // Create file with many lines to force multiple segments
    std::string content;
    for (int i = 0; i < 150; i++) {
        content += "Line " + std::to_string(i) + "\n";
    }

    std::string filename = createTestFile(content);
    wksp->load_file_to_segments(filename);

    // Position in middle
    wksp->position(75);

    // Break at middle
    int result = wksp->breaksegm(75, true);
    EXPECT_EQ(result, 0);

    // Delete some lines
    Segment *deleted = wksp->delete_segments(70, 80);

    // Cleanup
    if (deleted)
        Workspace::cleanup_segments(deleted);
    cleanupTestFile(filename);
}

//
// Test view management methods
//
TEST_F(WorkspaceTest, ScrollVerticalDown)
{
    wksp->set_nlines(100);
    wksp->set_topline(0);

    // Scroll down by 10 lines
    wksp->scroll_vertical(10, 20, 100);

    EXPECT_EQ(wksp->topline(), 10);
}

TEST_F(WorkspaceTest, ScrollVerticalUp)
{
    wksp->set_nlines(100);
    wksp->set_topline(20);

    // Scroll up by 10 lines
    wksp->scroll_vertical(-10, 20, 100);

    EXPECT_EQ(wksp->topline(), 10);
}

TEST_F(WorkspaceTest, ScrollVerticalClampTop)
{
    wksp->set_nlines(100);
    wksp->set_topline(10);

    // Try to scroll up beyond top
    wksp->scroll_vertical(-20, 20, 100);

    EXPECT_GE(wksp->topline(), 0);
}

TEST_F(WorkspaceTest, ScrollVerticalClampBottom)
{
    wksp->set_nlines(100);
    wksp->set_topline(80);

    // Try to scroll down beyond bottom
    wksp->scroll_vertical(30, 20, 100);

    // Should clamp to last valid position
    EXPECT_LE(wksp->topline(), 100 - 20);
}

TEST_F(WorkspaceTest, ScrollHorizontalRight)
{
    wksp->set_basecol(0);

    // Scroll right by 10 columns
    wksp->scroll_horizontal(10, 80);

    EXPECT_EQ(wksp->basecol(), 10);
}

TEST_F(WorkspaceTest, ScrollHorizontalLeft)
{
    wksp->set_basecol(20);

    // Scroll left by 10 columns
    wksp->scroll_horizontal(-10, 80);

    EXPECT_EQ(wksp->basecol(), 10);
}

TEST_F(WorkspaceTest, ScrollHorizontalClamp)
{
    wksp->set_basecol(10);

    // Try to scroll left beyond 0
    wksp->scroll_horizontal(-20, 80);

    EXPECT_GE(wksp->basecol(), 0);
}

TEST_F(WorkspaceTest, GotoLine)
{
    wksp->set_nlines(100);
    wksp->set_topline(0);

    // Go to line 50
    wksp->goto_line(50, 20);

    EXPECT_EQ(wksp->line(), 50);
    EXPECT_LE(wksp->topline(), 50);
}

TEST_F(WorkspaceTest, GotoLineNearEnd)
{
    wksp->set_nlines(100);
    wksp->set_topline(0);

    // Go to line 95 (near end)
    wksp->goto_line(95, 20);

    EXPECT_EQ(wksp->line(), 95);
}

TEST_F(WorkspaceTest, UpdateToplineAfterInsert)
{
    wksp->set_nlines(100);
    wksp->set_topline(80);

    // Insert 5 lines at position 50
    wksp->update_topline_after_edit(50, 55, 5);

    // Since topline > edit position, should shift down
    EXPECT_GE(wksp->topline(), 85);
}

TEST_F(WorkspaceTest, UpdateToplineAfterDelete)
{
    wksp->set_nlines(100);
    wksp->set_topline(80);

    // Delete 5 lines starting at position 50
    wksp->update_topline_after_edit(50, 55, -5);

    // Since topline > edit position, should shift up
    EXPECT_LE(wksp->topline(), 75);
}

TEST_F(WorkspaceTest, UpdateToplineBeforeEdit)
{
    wksp->set_nlines(100);
    wksp->set_topline(60);

    // Edit happens at position 80 (below topline)
    wksp->update_topline_after_edit(80, 85, 5);

    // Since topline < edit position, should not change
    EXPECT_EQ(wksp->topline(), 60);
}

TEST_F(WorkspaceTest, WriteLineToTempAndSave)
{
    // Create a test file with 3 lines
    std::string filename = createTestFile("line1\nline2\nline3\n");

    // Load workspace from file
    wksp->load_file_to_segments(filename);

    // Verify initial load
    EXPECT_TRUE(wksp->has_segments());
    EXPECT_EQ(wksp->nlines(), 3);

    // Create a temp segment with modified line content
    // Use tempfile to write the modified line
    Segment *temp_seg = tempfile->write_line_to_temp("modified_line2");
    ASSERT_NE(temp_seg, nullptr);
    EXPECT_EQ(temp_seg->nlines, 1);
    EXPECT_NE(temp_seg->fdesc, 0); // Should not be 0
    EXPECT_GT(temp_seg->fdesc, 0); // Should be a valid file descriptor

    // Add assertion: fdesc should never be 0 for any segment in the workspace
    Segment *check_seg = wksp->chain();
    while (check_seg) {
        if (check_seg->nlines > 0) { // Skip tail segment with nlines=0
            EXPECT_NE(check_seg->fdesc, 0)
                << "Segment has fdesc=0, which is invalid for non-empty segments";
        }
        check_seg = check_seg->next;
    }

    // Insert temp segment to replace line 1 (0-indexed, so line1->modified_line2)
    // First break at line 1
    EXPECT_EQ(wksp->breaksegm(1, true), 0);
    Segment *old_seg = wksp->cursegm();
    Segment *prev    = old_seg ? old_seg->prev : nullptr;

    std::cout << "After first breaksegm: old_seg nlines=" << (old_seg ? old_seg->nlines : 0)
              << std::endl;

    // Break at line 2 to isolate line 1
    EXPECT_EQ(wksp->breaksegm(2, false), 0);
    Segment *after = wksp->cursegm();

    std::cout << "After second breaksegm: after nlines=" << (after ? after->nlines : 0)
              << std::endl;

    // Link new segment in place of old line
    temp_seg->prev = prev;
    temp_seg->next = after;

    if (prev) {
        prev->next = temp_seg;
    } else {
        wksp->set_chain(temp_seg);
    }

    if (after) {
        after->prev = temp_seg;
    }

    // Update workspace position
    wksp->set_cursegm(temp_seg);
    wksp->set_segmline(1);

    // Delete old segment
    delete old_seg;

    // Save to new file
    std::string out_filename = filename + ".out";
    cleanupTestFile(out_filename);

    bool saved = wksp->write_segments_to_file(out_filename);
    EXPECT_TRUE(saved);

    // Verify output file exists
    std::ifstream in(out_filename);
    EXPECT_TRUE(in.good());

    // Read and verify content line by line
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    in.close();

    // Debug: print actual content
    for (size_t i = 0; i < lines.size(); ++i) {
        std::cout << "Line " << i << ": '" << lines[i] << "'" << std::endl;
    }

    // Verify we have 3 lines
    EXPECT_EQ(lines.size(), 3);
    if (lines.size() >= 3) {
        EXPECT_EQ(lines[0], "line1");
        EXPECT_EQ(lines[1], "modified_line2");
        EXPECT_EQ(lines[2], "line3");
    }

    // Cleanup
    cleanupTestFile(filename);
    cleanupTestFile(out_filename);
}

TEST_F(WorkspaceTest, BuildSegmentChainFromLines)
{
    // Test that build_segments_from_lines writes to temp file
    std::vector<std::string> lines = { "First line", "Second line", "Third line" };

    wksp->build_segments_from_lines(lines);

    // Verify segments were created
    EXPECT_TRUE(wksp->has_segments());
    EXPECT_EQ(wksp->nlines(), 3);

    // Verify fdesc is not 0 (should be temp file fd)
    Segment *seg = wksp->chain();
    ASSERT_NE(seg, nullptr);
    EXPECT_GT(seg->fdesc, 0); // Should be tempfile_fd_
    EXPECT_EQ(seg->nlines, 3);

    // Verify all line sizes are stored
    EXPECT_EQ(seg->sizes.size(), 3);
    EXPECT_EQ(seg->sizes[0], 11); // "First line\n" = 11 bytes
    EXPECT_EQ(seg->sizes[1], 12); // "Second line\n" = 12 bytes
    EXPECT_EQ(seg->sizes[2], 11); // "Third line\n" = 11 bytes

    // Verify we can read lines back
    std::string line1 = wksp->read_line_from_segment(0);
    std::string line2 = wksp->read_line_from_segment(1);
    std::string line3 = wksp->read_line_from_segment(2);

    EXPECT_EQ(line1, "First line");
    EXPECT_EQ(line2, "Second line");
    EXPECT_EQ(line3, "Third line");

    // Verify we can save to file
    std::string out_filename = "build_segment_test.txt";
    cleanupTestFile(out_filename);

    bool saved = wksp->write_segments_to_file(out_filename);
    EXPECT_TRUE(saved);

    // Verify output file content
    std::ifstream in(out_filename);
    EXPECT_TRUE(in.good());

    std::vector<std::string> output_lines;
    std::string line;
    while (std::getline(in, line)) {
        output_lines.push_back(line);
    }
    in.close();

    EXPECT_EQ(output_lines.size(), 3);
    if (output_lines.size() == 3) {
        EXPECT_EQ(output_lines[0], "First line");
        EXPECT_EQ(output_lines[1], "Second line");
        EXPECT_EQ(output_lines[2], "Third line");
    }

    cleanupTestFile(out_filename);
}

TEST_F(WorkspaceTest, InsertSegmentsUpdatesNlines)
{
    // Test that insert_segments updates nlines correctly
    std::string filename = createTestFile("line0\nline1\nline2\n");
    wksp->load_file_to_segments(filename);

    EXPECT_EQ(wksp->nlines(), 3);

    // Create blank lines to insert
    Segment *blank_lines = Workspace::create_blank_lines(2);
    ASSERT_NE(blank_lines, nullptr);

    // Insert at line 1
    wksp->insert_segments(blank_lines, 1);

    // Verify line count increased
    EXPECT_EQ(wksp->nlines(), 5);

    // Verify we can read the lines
    EXPECT_EQ(wksp->read_line_from_segment(0), "line0");
    EXPECT_EQ(wksp->read_line_from_segment(1), "");
    EXPECT_EQ(wksp->read_line_from_segment(2), "");
    EXPECT_EQ(wksp->read_line_from_segment(3), "line1");
    EXPECT_EQ(wksp->read_line_from_segment(4), "line2");

    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, InsertSegmentsAtStart)
{
    // Test inserting at the beginning
    std::string filename = createTestFile("line0\nline1\n");
    wksp->load_file_to_segments(filename);

    EXPECT_EQ(wksp->nlines(), 2);

    Segment *blank_lines = Workspace::create_blank_lines(1);
    wksp->insert_segments(blank_lines, 0);

    // Should have 3 lines now
    EXPECT_EQ(wksp->nlines(), 3);

    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, DeleteSegmentsUpdatesNlines)
{
    // Test basic delete_segments functionality
    std::string filename = createTestFile("line0\nline1\nline2\n");
    wksp->load_file_to_segments(filename);

    EXPECT_EQ(wksp->nlines(), 3);

    // Delete lines 0-1
    Segment *deleted = wksp->delete_segments(0, 1);
    ASSERT_NE(deleted, nullptr);

    // Should have 1 line left
    EXPECT_EQ(wksp->nlines(), 1);

    // Should be line2
    EXPECT_EQ(wksp->read_line_from_segment(0), "line2");

    // Cleanup
    Workspace::cleanup_segments(deleted);
    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, DeleteSegmentsMiddle)
{
    // Test deleting from middle
    std::string filename = createTestFile("line0\nline1\nline2\nline3\n");
    wksp->load_file_to_segments(filename);

    EXPECT_EQ(wksp->nlines(), 4);

    // Delete middle lines
    Segment *deleted = wksp->delete_segments(1, 2);
    ASSERT_NE(deleted, nullptr);

    EXPECT_EQ(wksp->nlines(), 2);
    EXPECT_EQ(wksp->read_line_from_segment(0), "line0");
    EXPECT_EQ(wksp->read_line_from_segment(1), "line3");

    Workspace::cleanup_segments(deleted);
    cleanupTestFile(filename);
}

TEST_F(WorkspaceTest, InsertAndDeleteWorkflow)
{
    // Test complete workflow: load, insert, delete
    std::string filename = createTestFile("original0\noriginal1\noriginal2\n");
    wksp->load_file_to_segments(filename);

    EXPECT_EQ(wksp->nlines(), 3);

    // Insert 2 blank lines at line 0
    Segment *blank_lines = Workspace::create_blank_lines(2);
    wksp->insert_segments(blank_lines, 0);

    EXPECT_EQ(wksp->nlines(), 5);

    // Delete the first two lines (the blank ones we just inserted)
    Segment *deleted = wksp->delete_segments(0, 1);
    ASSERT_NE(deleted, nullptr);

    EXPECT_EQ(wksp->nlines(), 3);

    // Should be back to original content
    EXPECT_EQ(wksp->read_line_from_segment(0), "original0");
    EXPECT_EQ(wksp->read_line_from_segment(1), "original1");
    EXPECT_EQ(wksp->read_line_from_segment(2), "original2");

    Workspace::cleanup_segments(deleted);
    cleanupTestFile(filename);
}
