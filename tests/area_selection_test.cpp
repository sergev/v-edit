#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, AreaSelectionInCommandMode)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a test file with multiple lines
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    f << "Line 1 content\n";
    f << "Line 2 content\n";
    f << "Line 3 content\n";
    f << "Line 4 content\n";
    f.close();

    // Start editor with the test file
    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Verify we're in the main file
    std::string pane1 = capture_pane(session, -10);
    EXPECT_TRUE(pane1.find("Line 1 content") != std::string::npos)
        << "Expected to find 'Line 1 content'. Pane content: " << pane1;

    // Enter command mode (F1)
    send_keys(session, "F1");
    TmuxDriver::sleep_ms(200);

    // Verify command mode is active
    std::string pane2 = capture_pane(session, -10);
    EXPECT_TRUE(pane2.find("Cmd:") != std::string::npos)
        << "Expected to find 'Cmd:' in command mode. Pane content: " << pane2;

    // Press arrow key to start area selection
    send_keys(session, "Down");
    TmuxDriver::sleep_ms(200);

    // Verify area selection mode is active
    std::string pane3 = capture_pane(session, -10);
    EXPECT_TRUE(pane3.find("Area defined by cursor") != std::string::npos)
        << "Expected to find 'Area defined by cursor'. Pane content: " << pane3;

    // Move cursor to define area
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "Down");
    TmuxDriver::sleep_ms(100);
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(100);

    // Press Enter to finalize area selection
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(200);

    // Verify we're back to normal mode (not in command mode)
    std::string pane4 = capture_pane(session, -10);
    EXPECT_FALSE(pane4.find("Cmd:") != std::string::npos)
        << "Expected NOT to find 'Cmd:' after area selection. Pane content: " << pane4;

    kill_session(session);
    fs::remove(testFile);
}
