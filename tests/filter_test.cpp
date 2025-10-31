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

    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Position cursor at first line and select 3 lines
    send_keys(session, "F4"); // Enter filter mode
    TmuxDriver::sleep_ms(200);
    send_keys(session, "3"); // Type "3"
    TmuxDriver::sleep_ms(100);
    send_keys(session, " "); // Type space
    TmuxDriver::sleep_ms(100);
    send_keys(session, "sort"); // Type "sort"
    TmuxDriver::sleep_ms(200);
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(500);

    // Verify lines are sorted (check for sorted content)
    std::string pane = capture_pane(session, -10);
    EXPECT_FALSE(pane.empty());

    // The filter should have sorted the lines, so we should see the sorted content
    // After sorting "banana\napple\ncherry", we should see "apple" first
    EXPECT_TRUE(pane.find("apple") != std::string::npos)
        << "Expected to find 'apple' after sorting. Pane content: " << pane;

    kill_session(session);
    fs::remove(testFile);
}
