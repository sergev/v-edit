#include <gtest/gtest.h>

#include "TmuxDriver.h"

TEST_F(TmuxDriver, exit_without_save)
{
    const std::string testName    = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string sessionName = testName;
    const std::string appPath     = V_EDIT_BIN_PATH;

    create_session(sessionName, shell_quote(appPath));
    TmuxDriver::sleep_ms(200);

    // Enter command mode (^A), type 'qa', press Enter
    send_keys(sessionName, "C-a");
    send_keys(sessionName, "q");
    send_keys(sessionName, "a");
    send_keys(sessionName, "Enter");

    // Give the app a bit more time to terminate and for tmux to reflect it
    TmuxDriver::sleep_ms(700);

    const std::string pane = capture_pane(sessionName, -200);
    // If the session is gone, capture-pane returns empty; that's success.
    // If still present briefly, it may show the "Exiting" status; also success.
    if (!pane.empty()) {
        EXPECT_NE(pane.find("Exiting"), std::string::npos);
    } else {
        SUCCEED();
    }

    // Cleanup (in case session still exists)
    kill_session(sessionName);
}
