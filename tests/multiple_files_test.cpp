#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, MultipleFileSwitching)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    std::ofstream f(testFile);
    f << "Main file content\n";
    f << "Line 2 of main file\n";
    f.close();

    // Start editor with file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify we're in the main file
    std::string pane1 = capturePane(session, -10);
    EXPECT_TRUE(pane1.find("Main file content") != std::string::npos)
        << "Expected to find 'Main file content'. Pane content: " << pane1;

    // Switch to alternative workspace using ^N (which opens help)
    sendKeys(session, "C-n");
    TmuxDriver::sleepMs(500);

    // Verify we're now in the alternative workspace (help file)
    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("V-EDIT") != std::string::npos)
        << "Expected to find 'V-EDIT' in help. Pane content: " << pane2;

    // Switch back to main file using ^N
    sendKeys(session, "C-n");
    TmuxDriver::sleepMs(500);

    // Verify we're back in the main file
    std::string pane3 = capturePane(session, -10);
    EXPECT_TRUE(pane3.find("Main file content") != std::string::npos)
        << "Expected to find 'Main file content' after ^N. Pane content: " << pane3;

    // Switch to alternative workspace again using F3
    sendKeys(session, "F3");
    TmuxDriver::sleepMs(500);

    // Verify we're in alternative workspace again
    std::string pane4 = capturePane(session, -10);
    EXPECT_TRUE(pane4.find("V-EDIT") != std::string::npos)
        << "Expected to find 'V-EDIT' in help after F3. Pane content: " << pane4;

    killSession(session);
    fs::remove(testFile);
}
