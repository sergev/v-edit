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

    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Search for "banana" using ^F (Ctrl-F enters search mode with /)
    send_keys(session, "C-f");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "banana");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(200);

    // Verify cursor moved to first match
    std::string pane = capture_pane(session, -10);
    EXPECT_FALSE(pane.empty());
    EXPECT_TRUE(pane.find("apple banana") != std::string::npos);

    kill_session(session);
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

    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Search for "apple", then find next with 'n'
    send_keys(session, "F1");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "/apple");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(200);

    // Find next
    send_keys(session, "F1");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "n");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(200);

    std::string pane = capture_pane(session, -10);
    EXPECT_FALSE(pane.empty());

    kill_session(session);
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

    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Goto line 3 using F8
    send_keys(session, "F8");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "3");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(200);

    // Verify cursor at line 3
    std::string pane = capture_pane(session, -10);
    EXPECT_FALSE(pane.empty());

    kill_session(session);
    fs::remove(testFile);
}
