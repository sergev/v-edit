#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, MultipleFileSwitching)
{
    const std::string testName  = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session   = testName;
    const std::string app       = V_EDIT_BIN_PATH;
    const std::string testFile1 = testName + "_file1.txt";
    const std::string testFile2 = testName + "_file2.txt";

    std::ofstream f1(testFile1);
    f1 << "File 1 content\n";
    f1 << "Line 2 of file 1\n";
    f1.close();

    std::ofstream f2(testFile2);
    f2 << "File 2 content\n";
    f2 << "Line 2 of file 2\n";
    f2.close();

    // Start editor with first file
    createSession(session, shellQuote(app) + " " + shellQuote(testFile1));
    TmuxDriver::sleepMs(300);

    // Verify we're in file 1
    std::string pane1 = capturePane(session, -10);
    EXPECT_TRUE(pane1.find("File 1 content") != std::string::npos)
        << "Expected to find 'File 1 content'. Pane content: " << pane1;

    // Open second file using command mode
    sendKeys(session, "F1"); // Enter command mode
    TmuxDriver::sleepMs(200);
    sendKeys(session, "o" + testFile2); // Open second file
    TmuxDriver::sleepMs(200);
    sendKeys(session, "Enter");
    TmuxDriver::sleepMs(500);

    // Verify we're now in file 2
    std::string pane2 = capturePane(session, -10);
    EXPECT_TRUE(pane2.find("File 2 content") != std::string::npos)
        << "Expected to find 'File 2 content'. Pane content: " << pane2;

    // Switch back to file 1 using F3
    sendKeys(session, "F3"); // Next file
    TmuxDriver::sleepMs(500);

    // Verify we're back in file 1
    std::string pane3 = capturePane(session, -10);
    EXPECT_TRUE(pane3.find("File 1 content") != std::string::npos)
        << "Expected to find 'File 1 content' after F3. Pane content: " << pane3;

    // Switch to file 2 again using F3
    sendKeys(session, "F3"); // Next file
    TmuxDriver::sleepMs(500);

    // Verify we're back in file 2
    std::string pane4 = capturePane(session, -10);
    EXPECT_TRUE(pane4.find("File 2 content") != std::string::npos)
        << "Expected to find 'File 2 content' after second F3. Pane content: " << pane4;

    killSession(session);
    fs::remove(testFile1);
    fs::remove(testFile2);
}
