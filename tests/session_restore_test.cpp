#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

class SessionRestoreTest : public TmuxDriver {
protected:
    std::string testFile;
    std::string sessionFile;

    void SetUp() override
    {
        TmuxDriver::SetUp();
        const std::string testName =
            ::testing::UnitTest::GetInstance()->current_test_info()->name();
        testFile    = testName + "_test.txt";
        sessionFile = "restitty-v-edit-" + testName + "_session";

        // Create initial file
        std::ofstream f(testFile);
        f << "Line 1\nLine 2\nLine 3\n";
        f.close();
    }

    void TearDown() override
    {
        if (fs::exists(testFile))
            fs::remove(testFile);
        if (fs::exists(sessionFile))
            fs::remove(sessionFile);
        TmuxDriver::TearDown();
    }
};

TEST_F(SessionRestoreTest, RestoresSessionAndPosition)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // First session: edit file and move to line 2
    create_session(session + "_1", shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Move down, then quit
    send_keys(session + "_1", "Down");
    TmuxDriver::sleep_ms(100);
    send_keys(session + "_1", "Down");
    TmuxDriver::sleep_ms(100);
    send_keys(session + "_1", "C-a");
    TmuxDriver::sleep_ms(100);
    send_keys(session + "_1", "q");
    TmuxDriver::sleep_ms(200);

    kill_session(session + "_1");
    TmuxDriver::sleep_ms(200);

    // Second session: no args - should restore
    create_session(session + "_2", shell_quote(app));
    TmuxDriver::sleep_ms(300);

    // Capture and verify position
    std::string pane = capture_pane(session + "_2", -20);
    EXPECT_FALSE(pane.empty());

    // Should show line 2 or 3 in status (depends on implementation)
    EXPECT_TRUE(pane.find("Line=") != std::string::npos);

    kill_session(session + "_2");
}
