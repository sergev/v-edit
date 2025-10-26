#include <gtest/gtest.h>

#include "TmuxDriver.h"

TEST_F(TmuxDriver, exit_without_save)
{
    const std::string testName    = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string sessionName = testName;
    const std::string appPath     = V_EDIT_BIN_PATH;

    createSession(sessionName, shellQuote(appPath));
    TmuxDriver::sleepMs(200);

    // Enter command mode (^A), type 'qa', press Enter
    sendKeys(sessionName, "C-a");
    sendKeys(sessionName, "q");
    sendKeys(sessionName, "a");
    sendKeys(sessionName, "Enter");

    // Give the app a bit more time to terminate and for tmux to reflect it
    TmuxDriver::sleepMs(700);

    const std::string pane = capturePane(sessionName, -200);
    // If the session is gone, capture-pane returns empty; that's success.
    // If still present briefly, it may show the "Exiting" status; also success.
    if (!pane.empty()) {
        EXPECT_NE(pane.find("Exiting"), std::string::npos);
    } else {
        SUCCEED();
    }

    // Cleanup (in case session still exists)
    killSession(sessionName);
}
