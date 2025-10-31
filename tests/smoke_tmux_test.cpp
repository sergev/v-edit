#include <algorithm>
#include <cctype>

#include "TmuxDriver.h"

TEST_F(TmuxDriver, PaneHasPrintableOutput)
{
    const std::string testName    = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string sessionName = testName;
    const std::string appPath     = V_EDIT_BIN_PATH; // provided via CMake

    create_session(sessionName, shell_quote(appPath));
    TmuxDriver::sleep_ms(200);

    const std::string pane = capture_pane(sessionName, -200);
    ASSERT_FALSE(pane.empty());

    const bool hasPrintable =
        std::any_of(pane.begin(), pane.end(), [](unsigned char c) { return std::isprint(c) != 0; });
    EXPECT_TRUE(hasPrintable);

    kill_session(sessionName);
}
