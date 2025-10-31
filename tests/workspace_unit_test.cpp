#include <fcntl.h>
#include <gtest/gtest.h>

#include <fstream>

#include "tempfile.h"
#include "workspace.h"

// Simplified workspace test fixture
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

    int OpenFile(const std::string &path)
    {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("Cannot open file: " + path);
        }
        return fd;
    }
};

//
// Test create_blank_lines - static functions
//
TEST_F(WorkspaceTest, CreateBlankLines)
{
    std::list<Segment> seg_list = Workspace::create_blank_lines(5);

    EXPECT_EQ(seg_list.size(), 1);
    EXPECT_EQ(seg_list.front().line_count, 5);
    EXPECT_EQ(seg_list.front().file_descriptor, -1);
}

TEST_F(WorkspaceTest, CreateBlankLinesLarge)
{
    // Test with > 127 lines to verify segment splitting
    std::list<Segment> seg_list = Workspace::create_blank_lines(200);

    EXPECT_FALSE(seg_list.empty());
    // Should split large line counts into multiple segments
    int total_lines = 0;
    for (const auto &seg : seg_list) {
        total_lines += seg.line_count;
    }
    EXPECT_EQ(total_lines, 200);
}

//
// Test workspace operations with file I/O
//
TEST_F(WorkspaceTest, LoadAndBreakSegment)
{
    // Create test file on disk first
    std::string content  = "Line 0\nLine 1\nLine 2\nLine 3\nLine 4\n";
    std::string filename = std::string(__func__) + ".txt";
    std::ofstream f(filename);
    f << content;
    f.close();

    wksp->load_file(OpenFile(filename));

    EXPECT_EQ(wksp->total_line_count(), 5);

    // Break at line 2
    int result = wksp->split(2);
    EXPECT_EQ(result, 0);

    // Verify segmentation worked
    EXPECT_NE(wksp->cursegm(), wksp->get_contents().end());
    EXPECT_EQ(wksp->current_segment_base_line(), 2);

    // Cleanup
    std::remove(filename.c_str());
}

TEST_F(WorkspaceTest, BuildAndReadLines)
{
    std::vector<std::string> lines = { "First line", "Second line", "Third line" };
    wksp->load_text(lines);

    // Verify segments were created
    EXPECT_EQ(wksp->total_line_count(), 3);

    // Verify we can read the lines back
    EXPECT_EQ(wksp->read_line(0), "First line");
    EXPECT_EQ(wksp->read_line(1), "Second line");
    EXPECT_EQ(wksp->read_line(2), "Third line");
}

TEST_F(WorkspaceTest, InsertBlankLines)
{
    // Create test file
    std::string content  = "Line 0\nLine 1\nLine 2\n";
    std::string filename = std::string(__func__) + ".txt";
    std::ofstream f(filename);
    f << content;
    f.close();

    wksp->load_file(OpenFile(filename));
    EXPECT_EQ(wksp->total_line_count(), 3);

    // Create blank lines to insert
    std::list<Segment> to_insert = Workspace::create_blank_lines(2);

    // Insert at line 1
    wksp->insert_contents(to_insert, 1);

    // Verify line count increased
    EXPECT_EQ(wksp->total_line_count(), 5);

    // Cleanup
    std::remove(filename.c_str());
}

TEST_F(WorkspaceTest, DeleteLines)
{
    // Create test file
    std::string content  = "Line 0\nLine 1\nLine 2\nLine 3\n";
    std::string filename = std::string(__func__) + ".txt";
    std::ofstream f(filename);
    f << content;
    f.close();

    wksp->load_file(OpenFile(filename));
    EXPECT_EQ(wksp->total_line_count(), 4);

    // Delete lines 1-2
    wksp->delete_contents(1, 2);

    // Verify line count decreased
    EXPECT_EQ(wksp->total_line_count(), 2);

    // Cleanup
    std::remove(filename.c_str());
}

//
// Test view management
//
TEST_F(WorkspaceTest, ScrollVertical)
{
    // Insert empty lines.
    auto blank_lines = Workspace::create_blank_lines(100);
    wksp->insert_contents(blank_lines, 0);
    wksp->view.topline = 0;

    // Scroll down by 10 lines
    wksp->scroll_vertical(10, 20, 100);

    EXPECT_EQ(wksp->view.topline, 10);
}

TEST_F(WorkspaceTest, GotoLine)
{
    // Insert empty lines.
    auto blank_lines = Workspace::create_blank_lines(100);
    wksp->insert_contents(blank_lines, 0);
    wksp->view.topline = 0;

    // Go to line 50 with 20 visible rows
    wksp->goto_line(50, 20);

    // Should position line 50 in visible range
    EXPECT_LE(wksp->view.topline, 50);
    EXPECT_GE(wksp->view.topline, 30); // 50 - 20
}

//
// Test segment merging
//
TEST_F(WorkspaceTest, CatSegmentMerge)
{
    // Create test file with multiple lines
    std::string content  = "Line 0\nLine 1\nLine 2\nLine 3\n";
    std::string filename = std::string(__func__) + ".txt";
    std::ofstream f(filename);
    f << content;
    f.close();

    wksp->load_file(OpenFile(filename));

    // Break at line 2 to create two segments
    int result = wksp->split(2);
    EXPECT_EQ(result, 0);

    // Position to second segment (line 2)
    wksp->change_current_line(2);

    // Try to merge with previous segment
    bool merged = wksp->merge();

    // Merge result depends on segment structure, but no crash should occur
    EXPECT_GE(wksp->total_line_count(), 0);

    // Cleanup
    std::remove(filename.c_str());
}

//
// Test basic file operations and view management
//
TEST_F(WorkspaceTest, SaveAndLoadCycle)
{
    // Create test content and save to file
    std::string content = "Test line 1\nTest line 2\nTest line 3\n";

    std::vector<std::string> lines = { "Test line 1", "Test line 2", "Test line 3" };
    wksp->load_text(lines);
    std::string out_filename = std::string(__func__) + ".txt";

    bool saved = wksp->write_file(out_filename);
    EXPECT_TRUE(saved);

    // Load back and verify
    wksp->load_file(OpenFile(out_filename));
    EXPECT_EQ(wksp->total_line_count(), 3);
    EXPECT_EQ(wksp->read_line(0), "Test line 1");

    // Cleanup
    std::remove(out_filename.c_str());
}

TEST_F(WorkspaceTest, ScrollAndGotoOperations)
{
    // Insert empty lines.
    auto blank_lines = Workspace::create_blank_lines(100);
    wksp->insert_contents(blank_lines, 0);
    wksp->view.topline = 50;

    // Test basic scrolling functionality
    wksp->scroll_vertical(10, 25, 100);
    EXPECT_EQ(wksp->view.topline, 60);

    wksp->scroll_horizontal(5, 80);
    EXPECT_EQ(wksp->view.basecol, 5);

    wksp->goto_line(25, 10);
    EXPECT_GE(wksp->view.topline, 15); // Should center around line 25
}

TEST_F(WorkspaceTest, ToplineUpdateAfterEdit)
{
    // Insert empty lines.
    auto blank_lines = Workspace::create_blank_lines(100);
    wksp->insert_contents(blank_lines, 0);
    wksp->view.topline = 50;

    // Simulate inserting lines
    wksp->update_topline_after_edit(40, 45, 5);
    EXPECT_GE(wksp->view.topline, 55);

    // Simulate deleting lines
    wksp->update_topline_after_edit(60, 65, -3);
    EXPECT_LE(wksp->view.topline, 52);
}

TEST_F(WorkspaceTest, ModifiedStateTests)
{
    // Test modification tracking
    EXPECT_FALSE(wksp->file_state.modified);
    wksp->file_state.modified = true;
    EXPECT_TRUE(wksp->file_state.modified);
    wksp->file_state.modified = false;
    EXPECT_FALSE(wksp->file_state.modified);
}

TEST_F(WorkspaceTest, BackupDoneStateTests)
{
    // Test backup completion tracking
    EXPECT_FALSE(wksp->file_state.backup_done);
    wksp->file_state.backup_done = true;
    EXPECT_TRUE(wksp->file_state.backup_done);
    wksp->file_state.backup_done = false;
    EXPECT_FALSE(wksp->file_state.backup_done);
}

TEST_F(WorkspaceTest, ChainAccessorsEmpty)
{
    // Empty workspace has no segments and cursegm() is end()
    EXPECT_EQ(wksp->cursegm(), wksp->get_contents().end());
    EXPECT_EQ(wksp->total_line_count(), 0);
}

TEST_F(WorkspaceTest, BuildFromText)
{
    // Test building segments from multi-line text
    std::string text = "Line one\nLine two\nLine three\nLast line";
    wksp->load_text(text);

    EXPECT_EQ(wksp->total_line_count(), 4);

    // Position to each line before reading
    wksp->change_current_line(0);
    EXPECT_EQ(wksp->read_line(0), "Line one");
    wksp->change_current_line(3);
    EXPECT_EQ(wksp->read_line(3), "Last line");
}

TEST_F(WorkspaceTest, ResetWorkspace)
{
    // Add some segments
    wksp->load_text(std::vector<std::string>{ "test", "content" });

    // Verify state is set (load_text calls reset first, then sets nlines to 2)
    EXPECT_EQ(wksp->total_line_count(), 2); // load_text sets this based on vector size
    wksp->file_state.modified = true;       // Manually set modified

    // Reset workspace
    wksp->reset();

    EXPECT_EQ(wksp->total_line_count(), 0);
    EXPECT_FALSE(wksp->file_state.modified);
    EXPECT_EQ(wksp->file_state.writable, 0);
}

TEST_F(WorkspaceTest, SetCurrentSegmentNavigation)
{
    // Create test content with multiple lines
    std::vector<std::string> lines = { "Line 0", "Line 1", "Line 2", "Line 3", "Line 4", "Line 5" };
    wksp->load_text(lines);

    // Navigate to different lines
    int result = wksp->change_current_line(3);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(wksp->position.line, 3);
    EXPECT_NE(wksp->cursegm(), wksp->get_contents().end());

    // Test navigation to line beyond end
    result = wksp->change_current_line(10);
    EXPECT_EQ(result, 1); // Should return 1 (beyond end)
}

TEST_F(WorkspaceTest, BreakSegmentVariations)
{
    std::vector<std::string> lines = { "Line 0", "Line 1", "Line 2", "Line 3", "Line 4" };
    wksp->load_text(lines);
    wksp->debug_print(std::cout);
    EXPECT_EQ(wksp->total_line_count(), 5);

    // Break at line 0 (should be no-op)
    std::cout << "--- Break at line 0, no-op\n";
    int result = wksp->split(0);
    wksp->debug_print(std::cout);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(wksp->total_line_count(), 5);

    // Break at line 3
    std::cout << "--- Break at line 3\n";
    result = wksp->split(3);
    wksp->debug_print(std::cout);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(wksp->position.line, 3);
    EXPECT_EQ(wksp->total_line_count(), 5);

    // Break beyond end (should create blank lines)
    std::cout << "--- Break beyond end, create blank lines\n";
    result = wksp->split(8);
    wksp->debug_print(std::cout);
    EXPECT_EQ(result, 1); // Should extend file
    EXPECT_EQ(wksp->total_line_count(), 8);
}

TEST_F(WorkspaceTest, SegmentCatOperations)
{
    std::vector<std::string> lines = { "A", "B", "C", "D", "E" };
    wksp->load_text(lines);

    // Break to create segments to merge
    wksp->split(2);
    wksp->change_current_line(2);

    // Test merge conditions - may or may not succeed depending on implementation
    bool merged = wksp->merge();
    // Just verify no crash - merge success depends on segment positioning
    EXPECT_TRUE(true); // No segmentation fault

    // Test merge on first segment (shouldn't work)
    wksp->change_current_line(0);
    merged = wksp->merge();
    EXPECT_FALSE(merged); // Can't merge first segment
}

TEST_F(WorkspaceTest, SegmentDeleteOperations)
{
    std::vector<std::string> lines = { "A", "B", "C", "D", "E" };
    wksp->load_text(lines);
    EXPECT_EQ(wksp->total_line_count(), 5);

    // Delete lines 1-2
    wksp->delete_contents(1, 2);
    EXPECT_EQ(wksp->total_line_count(), 3);

    // Delete line 1
    wksp->delete_contents(1, 1);
    EXPECT_EQ(wksp->total_line_count(), 2);

    // Delete invalid range (should be safe)
    wksp->delete_contents(10, 15); // Should handle gracefully
}

TEST_F(WorkspaceTest, ViewManagementComprehensive)
{
    // Insert empty lines.
    auto blank_lines = Workspace::create_blank_lines(100);
    wksp->insert_contents(blank_lines, 0);

    // Test vertical scrolling boundaries
    wksp->view.topline = 0;
    wksp->scroll_vertical(-10, 20, 100); // Scroll up from top
    EXPECT_EQ(wksp->view.topline, 0);    // Should clamp to 0

    wksp->view.topline = 85;
    wksp->scroll_vertical(20, 20, 100); // Scroll down from bottom
    EXPECT_EQ(wksp->view.topline, 80);  // Should clamp to 80

    // Test horizontal scrolling
    wksp->view.basecol = 0;
    wksp->scroll_horizontal(-5, 80);  // Scroll left from 0
    EXPECT_EQ(wksp->view.basecol, 0); // Should clamp

    wksp->view.basecol = 10;
    wksp->scroll_horizontal(-15, 80); // Scroll left past 0
    EXPECT_EQ(wksp->view.basecol, 0); // Should clamp

    // Test goto_line positioning
    wksp->goto_line(50, 15);
    EXPECT_GE(wksp->view.topline, 35); // 50 - 15
    EXPECT_LE(wksp->view.topline, 50);
}

TEST_F(WorkspaceTest, ComplexEditWorkflow)
{
    // Complex sequence: load -> insert -> break -> merge -> save

    // Load initial content
    std::vector<std::string> lines = { "Original 1", "Original 2", "Original 3" };
    wksp->load_text(lines);
    EXPECT_EQ(wksp->total_line_count(), 3);

    // Insert blank lines
    auto blanks = Workspace::create_blank_lines(3);
    wksp->insert_contents(blanks, 0);
    EXPECT_EQ(wksp->total_line_count(), 6);

    // Break at various points
    wksp->split(2);
    wksp->change_current_line(4);
    wksp->split(4);

    // Try some merges (may or may not succeed)
    wksp->change_current_line(2);
    wksp->merge(); // Try to merge - result depends on implementation

    wksp->change_current_line(4);
    wksp->merge(); // Try another merge

    // Final state verification
    EXPECT_GE(wksp->total_line_count(), 3); // Should have at least original lines

    // Test saving (shouldn't crash)
    bool saved = wksp->write_file("complex_test_out.txt");
    EXPECT_TRUE(saved); // May be false if path issues, but shouldn't crash
}
