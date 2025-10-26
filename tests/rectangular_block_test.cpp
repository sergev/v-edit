#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, RectangularBlockCopyPaste)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create a test file with structured content
    std::ofstream f(testFile);
    f << "Line 1: abc123def456\n";
    f << "Line 2: ghi789jkl012\n";
    f << "Line 3: mno345pqr678\n";
    f << "Line 4: stu901vwx234\n";
    f.close();

    // Start editor with the test file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Move cursor to position (line 0, column 8) - before "123"
    for (int i = 0; i < 8; ++i) {
        sendKeys(session, "Right");
        TmuxDriver::sleepMs(50);
    }

    // Enter command mode
    sendKeys(session, "F1");
    TmuxDriver::sleepMs(200);

    // Move cursor to define rectangular area (down 2 lines, right 3 characters)
    sendKeys(session, "Down");
    TmuxDriver::sleepMs(50);
    sendKeys(session, "Down");
    TmuxDriver::sleepMs(50);
    sendKeys(session, "Right");
    TmuxDriver::sleepMs(50);
    sendKeys(session, "Right");
    TmuxDriver::sleepMs(50);
    sendKeys(session, "Right");
    TmuxDriver::sleepMs(50);

    // Copy the rectangular block with ^C (this also terminates selection)
    sendKeys(session, "C-c");
    TmuxDriver::sleepMs(200);

    // Move to a different location
    for (int i = 0; i < 10; ++i) {
        sendKeys(session, "Right");
        TmuxDriver::sleepMs(50);
    }

    // Paste the rectangular block (^V)
    sendKeys(session, "C-v");
    TmuxDriver::sleepMs(200);

    // Exit and save
    sendKeys(session, "C-a");
    sendKeys(session, "q");
    sendKeys(session, "a");
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(600);

    // Verify the file content
    std::ifstream in(testFile);
    ASSERT_TRUE(in.good());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // The file should have been modified
    EXPECT_GT(contents.length(), 0);

    // Cleanup
    fs::remove(testFile);
    killSession(session);
}

TEST_F(TmuxDriver, RectangularBlockDelete)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create a test file with structured content
    std::ofstream f(testFile);
    f << "Line 1: abc123def456\n";
    f << "Line 2: ghi789jkl012\n";
    f << "Line 3: mno345pqr678\n";
    f << "Line 4: stu901vwx234\n";
    f.close();

    // Start editor with the test file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Move cursor to position (line 0, column 8)
    for (int i = 0; i < 8; ++i) {
        sendKeys(session, "Right");
        TmuxDriver::sleepMs(50);
    }

    // Enter command mode
    sendKeys(session, "F1");
    TmuxDriver::sleepMs(200);

    // Move cursor to define rectangular area
    sendKeys(session, "Down");
    TmuxDriver::sleepMs(50);
    sendKeys(session, "Right");
    TmuxDriver::sleepMs(50);
    sendKeys(session, "Right");
    TmuxDriver::sleepMs(50);

    // Delete the rectangular block with ^Y (this also terminates selection)
    sendKeys(session, "C-y");
    TmuxDriver::sleepMs(200);

    // Exit and save
    sendKeys(session, "C-a");
    sendKeys(session, "q");
    sendKeys(session, "a");
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(600);

    // Verify the file content
    std::ifstream in(testFile);
    ASSERT_TRUE(in.good());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // The file should have been modified
    EXPECT_GT(contents.length(), 0);

    // Cleanup
    fs::remove(testFile);
    killSession(session);
}
