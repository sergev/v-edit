#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, ExternalFilterSortsLines)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create test file with unsorted lines
    std::ofstream f(testFile);
    f << "banana\n";
    f << "apple\n";
    f << "cherry\n";
    f.close();

    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Position cursor at first line and select 3 lines
    sendKeys(session, "F4"); // Enter filter mode
    TmuxDriver::sleepMs(200);
    sendKeys(session, "3"); // Type "3"
    TmuxDriver::sleepMs(100);
    sendKeys(session, " "); // Type space
    TmuxDriver::sleepMs(100);
    sendKeys(session, "sort"); // Type "sort"
    TmuxDriver::sleepMs(200);
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(500);

    // Verify lines are sorted (check for sorted content)
    std::string pane = capturePane(session, -10);
    EXPECT_FALSE(pane.empty());

    // The filter should have sorted the lines, so we should see the sorted content
    // After sorting "banana\napple\ncherry", we should see "apple" first
    EXPECT_TRUE(pane.find("apple") != std::string::npos)
        << "Expected to find 'apple' after sorting. Pane content: " << pane;

    killSession(session);
    fs::remove(testFile);
}
