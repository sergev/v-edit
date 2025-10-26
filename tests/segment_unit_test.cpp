#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "editor.h"

namespace fs = std::filesystem;

// Declare SegmentTest fixture
class SegmentTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        editor = std::make_unique<Editor>();
        // Manually initialize editor state needed for tests
        editor->files.clear();
        editor->files.resize(3);
        editor->wksp       = Editor::Workspace{};
        editor->wksp.wfile = 1;
        editor->open_files.clear();
        editor->current_file_index     = 0;
        editor->alternative_file_index = -1;
    }

    void TearDown() override { editor.reset(); }

    std::unique_ptr<Editor> editor;

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
};

TEST_F(SegmentTest, LoadFileToSegments)
{
    std::string filename = createTestFile("Line 1\nLine 2\nLine 3\n");

    editor->load_file_to_segments(filename);

    // Verify segments were created
    auto &files = editor->files;
    EXPECT_FALSE(files.empty());

    cleanupTestFile(filename);
}

TEST_F(SegmentTest, ReadLineFromSegment)
{
    std::string filename = createTestFile("First line\nSecond line\nThird line\n");

    editor->load_file_to_segments(filename);

    // Verify segments were created
    EXPECT_FALSE(editor->files.empty());
    EXPECT_FALSE(editor->files[editor->wksp.wfile].chain == nullptr);

    // Read first line
    std::string line1 = editor->read_line_from_segment(0);
    EXPECT_EQ("First line", line1);

    // Read second line
    std::string line2 = editor->read_line_from_segment(1);
    EXPECT_EQ("Second line", line2);

    // Read third line
    std::string line3 = editor->read_line_from_segment(2);
    EXPECT_EQ("Third line", line3);

    cleanupTestFile(filename);
}

TEST_F(SegmentTest, HandleEmptyFile)
{
    std::string filename = createTestFile("");

    editor->load_file_to_segments(filename);

    // Should handle empty file without crashing
    std::string line = editor->read_line_from_segment(0);
    EXPECT_EQ("", line);

    cleanupTestFile(filename);
}

TEST_F(SegmentTest, HandleLargeFile)
{
    // Create a file with 1000 lines
    std::string content;
    for (int i = 0; i < 1000; i++) {
        content += "Line " + std::to_string(i) + "\n";
    }

    std::string filename = createTestFile(content);

    editor->load_file_to_segments(filename);

    // Read first line
    std::string firstLine = editor->read_line_from_segment(0);
    EXPECT_EQ("Line 0", firstLine);

    // Read last line
    std::string lastLine = editor->read_line_from_segment(999);
    EXPECT_EQ("Line 999", lastLine);

    // Read middle line
    std::string middleLine = editor->read_line_from_segment(500);
    EXPECT_EQ("Line 500", middleLine);

    cleanupTestFile(filename);
}

TEST_F(SegmentTest, HandleVeryLongLines)
{
    // Create a file with very long lines
    std::string longLine;
    for (int i = 0; i < 200; i++) {
        longLine += "This is a very long line that contains many characters. ";
    }

    std::string content  = longLine + "\nSecond line\n";
    std::string filename = createTestFile(content);

    editor->load_file_to_segments(filename);

    // Read the long line
    std::string line1 = editor->read_line_from_segment(0);
    EXPECT_EQ(longLine, line1);

    // Read second line
    std::string line2 = editor->read_line_from_segment(1);
    EXPECT_EQ("Second line", line2);

    cleanupTestFile(filename);
}

TEST_F(SegmentTest, WriteSegmentsToFile)
{
    std::string filename = createTestFile("Original content\n");

    editor->load_file_to_segments(filename);

    // Write to a new file
    std::string outputFile = "output_" + filename;
    bool success           = editor->write_segments_to_file(outputFile);
    EXPECT_TRUE(success);

    // Verify content matches
    std::ifstream savedFile(outputFile);
    std::string firstLine;
    std::getline(savedFile, firstLine);
    EXPECT_EQ("Original content", firstLine);

    cleanupTestFile(filename);
    cleanupTestFile(outputFile);
}

TEST_F(SegmentTest, SegmentChainFromVariableLines)
{
    // Create a file with lines of different sizes to test segment boundaries
    // Lines should cross buffer boundaries to test the buffering logic
    std::string content;

    // Create lines that vary in length to test different cases
    for (int i = 0; i < 200; i++) {
        if (i % 4 == 0) {
            // Short line (7 chars): "Line X\n"
            content += "Line " + std::to_string(i) + "\n";
        } else if (i % 4 == 1) {
            // Medium line (15 chars): "Medium line X\n"
            content += "Medium line " + std::to_string(i) + "\n";
        } else if (i % 4 == 2) {
            // Longer line (23 chars): "This is a longer line X\n"
            content += "This is a longer line " + std::to_string(i) + "\n";
        } else {
            // Very long line (47+ chars)
            content +=
                "This is a very long line number " + std::to_string(i) + " with extra text\n";
        }
    }

    std::string filename = createTestFile(content);

    // Load file to segments
    editor->load_file_to_segments(filename);

    // Verify segments were created
    auto &files = editor->files;
    EXPECT_FALSE(files.empty());
    EXPECT_FALSE(files[editor->wksp.wfile].chain == nullptr);

    // Check segment chain structure
    auto *segment           = files[editor->wksp.wfile].chain;
    int segment_count       = 0;
    int total_segment_lines = 0;

    std::cout << "\n=== Segment Chain Analysis ===\n";

    // Store segments for detailed verification
    std::vector<Editor::Segment *> segments;
    while (segment) {
        segments.push_back(segment);
        segment = segment->next;
    }

    // Verify segment count
    EXPECT_EQ(segments.size(), 3);
    segment_count = segments.size();

    // Verify Segment 1
    EXPECT_EQ(segments[0]->nlines, 127);
    EXPECT_EQ(segments[0]->seek, 0);
    EXPECT_EQ(segments[0]->data.size(), 127);

    std::cout << "Segment 1:\n";
    std::cout << "  nlines: " << segments[0]->nlines << "\n";
    std::cout << "  seek: " << segments[0]->seek << "\n";
    std::cout << "  fdesc: " << segments[0]->fdesc << "\n";
    std::cout << "  data.size(): " << segments[0]->data.size() << "\n";

    // Verify Segment 2
    EXPECT_EQ(segments[1]->nlines, 73);
    EXPECT_EQ(segments[1]->seek, 3134);
    EXPECT_EQ(segments[1]->data.size(), 73);

    std::cout << "Segment 2:\n";
    std::cout << "  nlines: " << segments[1]->nlines << "\n";
    std::cout << "  seek: " << segments[1]->seek << "\n";
    std::cout << "  fdesc: " << segments[1]->fdesc << "\n";
    std::cout << "  data.size(): " << segments[1]->data.size() << "\n";

    // Verify Segment 3 (tail)
    EXPECT_EQ(segments[2]->nlines, 0);
    EXPECT_EQ(segments[2]->seek, 5040);
    EXPECT_EQ(segments[2]->data.size(), 0);

    std::cout << "Segment 3:\n";
    std::cout << "  nlines: " << segments[2]->nlines << "\n";
    std::cout << "  seek: " << segments[2]->seek << "\n";
    std::cout << "  fdesc: " << segments[2]->fdesc << "\n";
    std::cout << "  data.size(): " << segments[2]->data.size() << "\n";

    // Calculate total lines
    for (const auto *seg : segments) {
        total_segment_lines += seg->nlines;
        EXPECT_GE(seg->nlines, 0);
        EXPECT_LE(seg->nlines, 127); // Max lines per segment

        // Verify segment data is not empty (except for tail segment)
        if (seg->nlines > 0) {
            EXPECT_FALSE(seg->data.empty());
        }
    }

    std::cout << "\nTotal segments: " << segment_count << "\n";
    std::cout << "Total lines across all segments: " << total_segment_lines << "\n";
    std::cout << "================================\n\n";

    EXPECT_EQ(segment_count, 3);
    EXPECT_EQ(total_segment_lines, 200);

    // Verify we can read lines from each segment
    // Test various lines across the file
    EXPECT_EQ(editor->read_line_from_segment(0), "Line 0");
    EXPECT_EQ(editor->read_line_from_segment(1), "Medium line 1");
    EXPECT_EQ(editor->read_line_from_segment(3),
              "This is a very long line number 3 with extra text");
    EXPECT_EQ(editor->read_line_from_segment(100), "Line 100");
    EXPECT_EQ(editor->read_line_from_segment(199),
              "This is a very long line number 199 with extra text");

    cleanupTestFile(filename);
}
