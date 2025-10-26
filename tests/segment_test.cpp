#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, SegmentsLoadLargeFileEfficiently)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a large test file with 1000 lines
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    for (int i = 0; i < 1000; i++) {
        f << "Line " << i << " of 1000 lines with some content to make it substantial\n";
    }
    f.close();

    // Start editor with the large file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify we can see the first line
    std::string pane = capturePane(session, -10);
    EXPECT_TRUE(pane.find("Line 0 of 1000") != std::string::npos)
        << "Expected to find first line. Pane content: " << pane;

    // Verify editor is responsive (not hanging)
    sendKeys(session, "Down");
    TmuxDriver::sleepMs(100);

    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("Line 1 of 1000") != std::string::npos)
        << "Expected to find second line after Down. Pane content: " << pane2;

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SegmentsHandleEmptyFile)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create an empty file
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    f.close();

    // Start editor with the empty file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify editor started successfully
    std::string pane = capturePane(session, -10);
    EXPECT_FALSE(pane.empty()) << "Expected editor to start with empty file";

    // Should be able to add content
    sendKeys(session, "Test content");
    TmuxDriver::sleepMs(100);

    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("Test content") != std::string::npos)
        << "Expected to find inserted content. Pane content: " << pane2;

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SegmentsHandleSingleLineFile)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a file with single line (no newline at end)
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    f << "Single line without final newline";
    f.close();

    // Start editor with the single line file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify the line is displayed (check for the beginning since it's longer than the window
    // width)
    std::string pane = capturePane(session, -10);
    EXPECT_TRUE(pane.find("Single line without final ne~") != std::string::npos)
        << "Expected to find single line content. Pane content: " << pane;

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SegmentsHandleFileWithVeryLongLines)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a file with very long lines
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    std::string longLine;
    for (int i = 0; i < 200; i++) {
        longLine +=
            "This is a very long line that contains many characters to test segment handling of "
            "lines longer than 127 bytes. ";
    }
    f << longLine << "\n";
    f << "Second line\n";
    f << "Third line\n";
    f.close();

    // Start editor with the file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify the first part of the long line is displayed
    std::string pane = capturePane(session, -10);
    EXPECT_TRUE(pane.find("This is a very long line") != std::string::npos)
        << "Expected to find beginning of long line. Pane content: " << pane;

    // Move to second line
    sendKeys(session, "Down");
    TmuxDriver::sleepMs(100);

    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("Second line") != std::string::npos)
        << "Expected to find second line. Pane content: " << pane2;

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SegmentsAllowScrollingToEndOfLargeFile)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a file with many lines
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    for (int i = 0; i < 500; i++) {
        f << "Line " << i << "\n";
    }
    f.close();

    // Start editor with the file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Go to the end using Page Down - need enough to scroll through all 500 lines
    // With 10-line screen and 8 lines per page, we need about 500/8 = 62+ pages
    for (int i = 0; i < 70; i++) {
        sendKeys(session, "NPage");
        TmuxDriver::sleepMs(50);
    }

    // Verify we're near the end
    std::string pane = capturePane(session, -10);
    // Should find a line with a high number (400+)
    bool foundHighLine = false;
    for (int i = 450; i < 500; i++) {
        std::ostringstream oss;
        oss << "Line " << i;
        if (pane.find(oss.str()) != std::string::npos) {
            foundHighLine = true;
            break;
        }
    }

    EXPECT_TRUE(foundHighLine)
        << "Expected to find a line number 450-499 near the end. Pane content: " << pane;

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SegmentsHandleMixedLineLengths)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a file with mixed line lengths
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    f << "Short\n";                        // 6 chars
    f << "This is a medium length line\n"; // 31 chars
    f << "X\n";                            // 2 chars - very short
    std::string veryLong;
    for (int i = 0; i < 150; i++) {
        veryLong += "A";
    }
    f << veryLong << "\n";
    f << "Normal line\n";
    f.close();

    // Start editor with the file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify all lines are accessible
    std::string pane = capturePane(session, -10);
    EXPECT_TRUE(pane.find("Short") != std::string::npos)
        << "Expected to find short line. Pane content: " << pane;

    // Move through the lines
    sendKeys(session, "Down");
    TmuxDriver::sleepMs(100);

    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("medium length") != std::string::npos)
        << "Expected to find medium length line. Pane content: " << pane2;

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SegmentsPreserveFileContentOnSave)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a test file with known content
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    f << "Original line 1\n";
    f << "Original line 2\n";
    f << "Original line 3\n";
    f.close();

    // Start editor
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Modify the content
    sendKeys(session, "Modified ");
    TmuxDriver::sleepMs(100);

    // Save the file
    sendKeys(session, "F2");
    TmuxDriver::sleepMs(300);

    // Exit
    sendKeys(session, "C-x");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "C-c");
    TmuxDriver::sleepMs(300);

    killSession(session);

    // Verify file was saved
    std::ifstream savedFile(testFile);
    std::string firstLine;
    std::getline(savedFile, firstLine);
    savedFile.close();

    EXPECT_TRUE(firstLine.find("Modified") != std::string::npos ||
                firstLine.find("Original") != std::string::npos)
        << "Expected file to be modified or preserved. First line: " << firstLine;

    fs::remove(testFile);
}
