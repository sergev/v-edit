#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <fstream>

#include "tempfile.h"

// Tempfile test fixture
class TempfileTest : public ::testing::Test {
protected:
    void SetUp() override { tempfile = std::make_unique<Tempfile>(); }

    void TearDown() override { tempfile.reset(); }

    std::unique_ptr<Tempfile> tempfile;
};

// Test open_temp_file
TEST_F(TempfileTest, OpenTempFile)
{
    bool result = tempfile->open_temp_file();
    EXPECT_TRUE(result);
    EXPECT_GE(tempfile->fd(), 0);
}

// Test close_temp_file
TEST_F(TempfileTest, CloseTempFile)
{
    tempfile->open_temp_file();
    EXPECT_GE(tempfile->fd(), 0);

    tempfile->close_temp_file();
    EXPECT_EQ(tempfile->fd(), -1);
}

// Test write_line_to_temp with basic string
TEST_F(TempfileTest, WriteLineToTempBasic)
{
    auto segments = tempfile->write_line_to_temp("Hello World");

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 1);
    EXPECT_GE(seg.file_descriptor, 0); // Should have opened temp file
    // Segment struct no longer has prev/next pointers in std::list implementation
    EXPECT_EQ(seg.line_lengths.size(), 1);
    EXPECT_EQ(seg.line_lengths[0], 12); // "Hello World\n" = 12 bytes
    EXPECT_EQ(seg.file_offset, 0);      // First write should be at position 0
}

// Test write_line_to_temp with string already ending with newline
TEST_F(TempfileTest, WriteLineToTempWithNewline)
{
    auto segments = tempfile->write_line_to_temp("Already has newline\n");

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 1);
    EXPECT_EQ(seg.line_lengths.size(), 1);
    EXPECT_EQ(seg.line_lengths[0],
              20); // "Already has newline\n" = 20 bytes (newline already present)
}

// Test write_line_to_temp with empty string
TEST_F(TempfileTest, WriteLineToTempEmptyString)
{
    auto segments = tempfile->write_line_to_temp("");

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 1);
    EXPECT_EQ(seg.line_lengths.size(), 1);
    EXPECT_EQ(seg.line_lengths[0], 1); // Just '\n' = 1 byte
}

// Test multiple writes
TEST_F(TempfileTest, WriteLineToTempMultiple)
{
    auto seg1 = tempfile->write_line_to_temp("First line");
    auto seg2 = tempfile->write_line_to_temp("Second line");
    auto seg3 = tempfile->write_line_to_temp("Third line");

    ASSERT_EQ(seg1.size(), 1);
    ASSERT_EQ(seg2.size(), 1);
    ASSERT_EQ(seg3.size(), 1);

    const Segment &s1 = seg1.front();
    const Segment &s2 = seg2.front();
    const Segment &s3 = seg3.front();

    EXPECT_EQ(s1.file_offset, 0);
    EXPECT_EQ(s2.file_offset, 11);      // After "First line\n" (11 bytes)
    EXPECT_EQ(s3.file_offset, 11 + 12); // After "Second line\n" (12 bytes)

    EXPECT_EQ(s1.line_lengths[0], 11); // "First line\n" = 10 + 1 = 11 bytes
    EXPECT_EQ(s2.line_lengths[0], 12); // "Second line\n" = 11 + 1 = 12 bytes
    EXPECT_EQ(s3.line_lengths[0], 11); // "Third line\n" = 10 + 1 = 11 bytes
}

// Test that write opens temp file automatically if needed
TEST_F(TempfileTest, WriteLineToTempOpensFile)
{
    EXPECT_EQ(tempfile->fd(), -1); // Initially not open

    auto segments = tempfile->write_line_to_temp("Test");

    EXPECT_GE(tempfile->fd(), 0); // Should be opened now

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();
    EXPECT_EQ(seg.file_descriptor, tempfile->fd());
}

// Test reading back the written data
TEST_F(TempfileTest, WriteLineToTempVerifyContent)
{
    std::string test_content = "Test content for verification";
    auto segments            = tempfile->write_line_to_temp(test_content);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    // Seek to the segment position and read the data
    char buffer[256];
    lseek(seg.file_descriptor, seg.file_offset, SEEK_SET);
    int bytes_read = read(seg.file_descriptor, buffer, seg.line_lengths[0]);

    EXPECT_EQ(bytes_read, seg.line_lengths[0]);
    std::string read_content(buffer, bytes_read);
    EXPECT_EQ(read_content, test_content + "\n");
}

// Test write_line_to_temp with long line
TEST_F(TempfileTest, WriteLineToTempLongLine)
{
    std::string long_line(1000, 'A'); // 1000 'A' characters
    auto segments = tempfile->write_line_to_temp(long_line);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 1);
    EXPECT_EQ(seg.line_lengths[0], 1001); // 1000 'A's + '\n'

    // Verify content
    char buffer[1002];
    lseek(seg.file_descriptor, seg.file_offset, SEEK_SET);
    int bytes_read = read(seg.file_descriptor, buffer, seg.line_lengths[0]);

    EXPECT_EQ(bytes_read, 1001);
    std::string read_content(buffer, bytes_read);
    EXPECT_EQ(read_content, long_line + "\n");
}

// Test write_lines_to_temp for comparison
TEST_F(TempfileTest, WriteLineToTempVsWriteLines)
{
    std::vector<std::string> lines = { "Single line" };

    auto segments_lines = tempfile->write_lines_to_temp(lines);
    auto segments_line  = tempfile->write_line_to_temp("Single line");

    ASSERT_EQ(segments_lines.size(), 1);
    ASSERT_EQ(segments_line.size(), 1);

    const Segment &seg_lines = segments_lines.front();
    const Segment &seg_line  = segments_line.front();

    EXPECT_EQ(seg_lines.line_count, 1);
    EXPECT_EQ(seg_line.line_count, 1);
    EXPECT_EQ(seg_lines.line_lengths, seg_line.line_lengths);
}

// Test position tracking with multiple calls
TEST_F(TempfileTest, WriteLineToTempPositionTracking)
{
    // Start fresh
    tempfile->close_temp_file();
    bool opened = tempfile->open_temp_file();
    EXPECT_TRUE(opened);
    EXPECT_GE(tempfile->fd(), 0); // Should be opened

    // Write first line manually to ensure we have control
    auto seg1 = tempfile->write_line_to_temp("12345");
    auto seg2 = tempfile->write_line_to_temp("abcdef");
    auto seg3 = tempfile->write_line_to_temp("XYZ");

    ASSERT_EQ(seg1.size(), 1);
    ASSERT_EQ(seg2.size(), 1);
    ASSERT_EQ(seg3.size(), 1);

    const Segment &s1 = seg1.front();
    const Segment &s2 = seg2.front();
    const Segment &s3 = seg3.front();

    EXPECT_EQ(s1.file_offset, 0);
    EXPECT_EQ(s2.file_offset, s1.line_lengths[0]);
    EXPECT_EQ(s3.file_offset, s1.line_lengths[0] + s2.line_lengths[0]);

    EXPECT_EQ(s1.line_lengths[0], 6); // "12345\n"
    EXPECT_EQ(s2.line_lengths[0], 7); // "abcdef\n"
    EXPECT_EQ(s3.line_lengths[0], 4); // "XYZ\n"
}

// Test write_lines_to_temp with basic strings
TEST_F(TempfileTest, WriteLinesToTempBasic)
{
    std::vector<std::string> lines = { "First line", "Second line", "Third line" };

    auto segments = tempfile->write_lines_to_temp(lines);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 3);
    EXPECT_GE(seg.file_descriptor, 0); // Should have opened temp file
    // Segments are now in std::list - no prev/next pointers
    EXPECT_EQ(seg.line_lengths.size(), 3);
    EXPECT_EQ(seg.line_lengths[0], 11); // "First line\n" = 11 bytes
    EXPECT_EQ(seg.line_lengths[1], 12); // "Second line\n" = 12 bytes
    EXPECT_EQ(seg.line_lengths[2], 11); // "Third line\n" = 11 bytes
    EXPECT_EQ(seg.file_offset, 0);      // First write should be at position 0
}

// Test write_lines_to_temp with strings already ending with newlines
TEST_F(TempfileTest, WriteLinesToTempWithNewlines)
{
    std::vector<std::string> lines = { "Line with newline\n", "Another line" };

    auto segments = tempfile->write_lines_to_temp(lines);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 2);
    EXPECT_EQ(seg.line_lengths.size(), 2);
    EXPECT_EQ(seg.line_lengths[0],
              18); // "Line with newline\n" = 18 bytes (newline already present)
    EXPECT_EQ(seg.line_lengths[1], 13); // "Another line\n" = 13 bytes
}

// Test write_lines_to_temp with empty string in vector
TEST_F(TempfileTest, WriteLinesToTempEmptyStringInVector)
{
    std::vector<std::string> lines = { "", "Non-empty" };

    auto segments = tempfile->write_lines_to_temp(lines);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 2);
    EXPECT_EQ(seg.line_lengths.size(), 2);
    EXPECT_EQ(seg.line_lengths[0], 1);  // Just '\n' = 1 byte
    EXPECT_EQ(seg.line_lengths[1], 10); // "Non-empty\n" = 10 bytes
}

// Test write_lines_to_temp with empty vector
TEST_F(TempfileTest, WriteLinesToTempEmptyVector)
{
    std::vector<std::string> lines = {};

    auto segments = tempfile->write_lines_to_temp(lines);

    EXPECT_EQ(segments.size(), 0);
}

// Test write_lines_to_temp multiple calls
TEST_F(TempfileTest, WriteLinesToTempMultiple)
{
    std::vector<std::string> lines1 = { "First", "Second" };
    std::vector<std::string> lines2 = { "Third", "Fourth", "Fifth" };

    auto seg1 = tempfile->write_lines_to_temp(lines1);
    auto seg2 = tempfile->write_lines_to_temp(lines2);

    ASSERT_EQ(seg1.size(), 1);
    ASSERT_EQ(seg2.size(), 1);

    const Segment &s1 = seg1.front();
    const Segment &s2 = seg2.front();

    EXPECT_EQ(s1.file_offset, 0);
    EXPECT_EQ(s2.file_offset, 6 + 7); // After "First\n" (6) + "Second\n" (7) = 13

    EXPECT_EQ(s1.line_count, 2);
    EXPECT_EQ(s2.line_count, 3);
    EXPECT_EQ(s1.line_lengths.size(), 2);
    EXPECT_EQ(s2.line_lengths.size(), 3);
    EXPECT_EQ(s1.line_lengths[0], 6); // "First\n"
    EXPECT_EQ(s1.line_lengths[1], 7); // "Second\n"
    EXPECT_EQ(s2.line_lengths[0], 6); // "Third\n"
    EXPECT_EQ(s2.line_lengths[1], 7); // "Fourth\n"
    EXPECT_EQ(s2.line_lengths[2], 6); // "Fifth\n"
}

// Test that write_lines_to_temp opens temp file automatically if needed
TEST_F(TempfileTest, WriteLinesToTempOpensFile)
{
    EXPECT_EQ(tempfile->fd(), -1); // Initially not open

    std::vector<std::string> lines = { "Test line" };
    auto segments                  = tempfile->write_lines_to_temp(lines);

    EXPECT_GE(tempfile->fd(), 0); // Should be opened now

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();
    EXPECT_EQ(seg.file_descriptor, tempfile->fd());
}

// Test reading back the written data from write_lines_to_temp
TEST_F(TempfileTest, WriteLinesToTempVerifyContent)
{
    std::vector<std::string> lines = { "Line one", "Line two", "Line three" };
    auto segments                  = tempfile->write_lines_to_temp(lines);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    // Seek to the segment position and read the data
    char buffer[256];
    lseek(seg.file_descriptor, seg.file_offset, SEEK_SET);

    int total_bytes = 0;
    for (int size : seg.line_lengths) {
        total_bytes += size;
    }

    int bytes_read = read(seg.file_descriptor, buffer, total_bytes);

    EXPECT_EQ(bytes_read, total_bytes);
    std::string read_content(buffer, bytes_read);
    EXPECT_EQ(read_content, "Line one\nLine two\nLine three\n");
}

// Test write_lines_to_temp with mixed lines (some with newlines, some without)
TEST_F(TempfileTest, WriteLinesToTempMixedNewlines)
{
    std::vector<std::string> lines = { "Normal line", "Line with newline\n", "", "Another normal" };

    auto segments = tempfile->write_lines_to_temp(lines);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 4);
    EXPECT_EQ(seg.line_lengths.size(), 4);
    EXPECT_EQ(seg.line_lengths[0], 12); // "Normal line\n"
    EXPECT_EQ(seg.line_lengths[1], 18); // "Line with newline\n"
    EXPECT_EQ(seg.line_lengths[2], 1);  // "\n"
    EXPECT_EQ(seg.line_lengths[3], 15); // "Another normal\n"
}

// Test write_lines_to_temp with many lines
TEST_F(TempfileTest, WriteLinesToTempManyLines)
{
    std::vector<std::string> lines;
    for (int i = 0; i < 100; ++i) {
        lines.push_back("Line " + std::to_string(i));
    }

    auto segments = tempfile->write_lines_to_temp(lines);

    ASSERT_EQ(segments.size(), 1);
    const Segment &seg = segments.front();

    EXPECT_EQ(seg.line_count, 100);
    EXPECT_EQ(seg.line_lengths.size(), 100);

    // Each line should be "Line X\n" where X is the number
    // "Line 0\n" = 6 bytes, "Line 10\n" = 7 bytes, etc.
    for (int i = 0; i < 100; ++i) {
        std::string line = "Line " + std::to_string(i) + "\n";
        EXPECT_EQ(seg.line_lengths[i], line.size());
    }
}

// Test position tracking with mixed write_lines_to_temp and write_line_to_temp calls
TEST_F(TempfileTest, WriteLinesToTempPositionTracking)
{
    // Start fresh
    tempfile->close_temp_file();
    bool opened = tempfile->open_temp_file();
    EXPECT_TRUE(opened);
    EXPECT_GE(tempfile->fd(), 0);

    // Mixed calls: write_lines_to_temp then write_line_to_temp
    std::vector<std::string> lines_block1 = { "Block1 Line1", "Block1 Line2" };
    auto seg1                             = tempfile->write_lines_to_temp(lines_block1);

    auto seg2 = tempfile->write_line_to_temp("Single line");

    std::vector<std::string> lines_block2 = { "Block2 Line1" };
    auto seg3                             = tempfile->write_lines_to_temp(lines_block2);

    ASSERT_EQ(seg1.size(), 1);
    ASSERT_EQ(seg2.size(), 1);
    ASSERT_EQ(seg3.size(), 1);

    const Segment &s1 = seg1.front();
    const Segment &s2 = seg2.front();
    const Segment &s3 = seg3.front();

    EXPECT_EQ(s1.file_offset, 0);
    EXPECT_EQ(
        s2.file_offset,
        s1.file_offset + s1.line_lengths[0] + s1.line_lengths[1]);  // After both lines in block1
    EXPECT_EQ(s3.file_offset, s2.file_offset + s2.line_lengths[0]); // After the single line

    // Verify line_lengths
    EXPECT_EQ(s1.line_lengths[0], 13); // "Block1 Line1\n"
    EXPECT_EQ(s1.line_lengths[1], 13); // "Block1 Line2\n"
    EXPECT_EQ(s2.line_lengths[0], 12); // "Single line\n"
    EXPECT_EQ(s3.line_lengths[0], 13); // "Block2 Line1\n"
}
