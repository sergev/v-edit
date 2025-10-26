#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, SearchForwardFindsMatch)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create test file
    std::ofstream f(testFile);
    f << "apple banana\n";
    f << "cherry apple\n";
    f << "banana cherry\n";
    f.close();

    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Search for "banana" using ^F (Ctrl-F enters search mode with /)
    sendKeys(session, "C-f");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "banana");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(200);

    // Verify cursor moved to first match
    std::string pane = capturePane(session, -10);
    EXPECT_FALSE(pane.empty());
    EXPECT_TRUE(pane.find("apple banana") != std::string::npos);

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, SearchNextFindsSecondMatch)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create test file
    std::ofstream f(testFile);
    f << "apple banana\n";
    f << "cherry apple\n";
    f << "banana cherry\n";
    f.close();

    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Search for "apple", then find next with 'n'
    sendKeys(session, "F1");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "/apple");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(200);

    // Find next
    sendKeys(session, "F1");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "n");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(200);

    std::string pane = capturePane(session, -10);
    EXPECT_FALSE(pane.empty());

    killSession(session);
    fs::remove(testFile);
}

TEST_F(TmuxDriver, GotoLineCommandWorks)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create test file with 5 lines
    std::ofstream f(testFile);
    for (int i = 1; i <= 5; ++i) {
        f << "Line " << i << "\n";
    }
    f.close();

    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Goto line 3 using F8
    sendKeys(session, "F8");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "3");
    TmuxDriver::sleepMs(100);
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(200);

    // Verify cursor at line 3
    std::string pane = capturePane(session, -10);
    EXPECT_FALSE(pane.empty());

    killSession(session);
    fs::remove(testFile);
}
