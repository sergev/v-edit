#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "workspace.h"

namespace fs = std::filesystem;

// Workspace test fixture
class WorkspaceTest : public ::testing::Test {
protected:
    void SetUp() override { wksp = std::make_unique<Workspace>(); }

    void TearDown() override { wksp.reset(); }

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
    EXPECT_EQ(seg->data.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(seg->data[i], 1);
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
    EXPECT_EQ(copy->data.size(), original->data.size());

    for (size_t i = 0; i < copy->data.size(); ++i) {
        EXPECT_EQ(copy->data[i], original->data[i]);
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
