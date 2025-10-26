#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, HelpFileSystemOpensBuiltinHelp)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;

    // Create a test file
    const std::string testFile = testName + ".txt";
    std::ofstream f(testFile);
    f << "Main file content\n";
    f << "Line 2 of main file\n";
    f.close();

    // Start editor with the test file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile));
    TmuxDriver::sleepMs(300);

    // Verify we're in the main file
    std::string pane1 = capturePane(session, -10);
    EXPECT_TRUE(pane1.find("Main file content") != std::string::npos)
        << "Expected to find 'Main file content'. Pane content: " << pane1;

    // Switch to alternative workspace using ^N (should open help file)
    sendKeys(session, "C-n");
    TmuxDriver::sleepMs(500);

    // Verify that help content is displayed
    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("V-EDIT - Minimal Text Editor") != std::string::npos)
        << "Expected to find help content. Pane content: " << pane2;

    // Switch back to main file using ^N
    sendKeys(session, "C-n");
    TmuxDriver::sleepMs(500);

    // Verify we're back in the main file
    std::string pane3 = capturePane(session, -10);
    EXPECT_TRUE(pane3.find("Main file content") != std::string::npos)
        << "Expected to find 'Main file content' after switching back. Pane content: " << pane3;

    killSession(session);
    fs::remove(testFile);
}
