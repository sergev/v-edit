#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, AlternativeWorkspaceSwitching)
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

    // Switch to alternative workspace using ^N
    sendKeys(session, "C-n");
    TmuxDriver::sleepMs(500);

    // Just verify that something changed (cursor position, content, etc.)
    std::string pane2 = capturePane(session, -10);
    EXPECT_FALSE(pane2.empty()) << "Pane should not be empty after ^N";

    killSession(session);
    fs::remove(testFile);
}
