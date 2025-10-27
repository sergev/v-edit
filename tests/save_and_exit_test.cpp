#include <gtest/gtest.h>
#include <limits.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "TmuxDriver.h"

TEST_F(TmuxDriver, SaveAndExitWritesFile)
{
    const std::string testName    = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string sessionName = testName;
    const std::string appPath     = V_EDIT_BIN_PATH;
    const std::string fileName    = testName + ".txt";
    const std::string filePath    = fileName;
    std::remove(filePath.c_str());

    // Launch editor with file path
    createSession(sessionName, shellQuote(appPath + std::string(" ") + filePath));
    TmuxDriver::sleepMs(250);

    // Type lines: "hello", newline, "world", newline, "!"
    sendKeys(sessionName, "hello");
    sendKeys(sessionName, "Enter");
    sendKeys(sessionName, "world");
    sendKeys(sessionName, "Enter");
    sendKeys(sessionName, "!");
    TmuxDriver::sleepMs(100);

    // Save via F2
    sendKeys(sessionName, "F2");
    TmuxDriver::sleepMs(200);

    // Exit via ^A, qa, Enter
    sendKeys(sessionName, "C-a");
    sendKeys(sessionName, "q");
    sendKeys(sessionName, "a");
    sendKeys(sessionName, "Enter");
    TmuxDriver::sleepMs(600);

    // Verify file contents (allow a short delay for filesystem sync)
    TmuxDriver::sleepMs(400);
    std::ifstream in(filePath.c_str());
    ASSERT_TRUE(in.good()) << "File was not created: " << filePath;
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // Note: newline is appended by editor.
    EXPECT_EQ(contents, std::string("hello\nworld\n!\n"));

    // Cleanup
    std::remove(filePath.c_str());

    // Ensure tmux session is cleaned up in any case
    killSession(sessionName);
}

TEST_F(TmuxDriver, SaveCreatesBackupFileWithTildeSuffix)
{
    const std::string testName    = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string sessionName = testName;
    const std::string appPath     = V_EDIT_BIN_PATH;
    const std::string fileName    = testName + ".txt";
    const std::string backupName  = fileName + "~";
    const std::string originalContent = "original content\nline 2\nline 3";

    // Create original file with content
    std::ofstream out(fileName.c_str());
    out << originalContent;
    out.close();

    // Remove any existing backup
    std::remove(backupName.c_str());

    // Launch editor with existing file
    createSession(sessionName, shellQuote(appPath + std::string(" ") + fileName));
    TmuxDriver::sleepMs(250);

    // Make an edit - move to end and add a line
    sendKeys(sessionName, "End");
    sendKeys(sessionName, "Enter");
    sendKeys(sessionName, "modified line");
    TmuxDriver::sleepMs(100);

    // Save via F2 - this should create backup
    sendKeys(sessionName, "F2");
    TmuxDriver::sleepMs(200);

    // Exit via ^A, qa, Enter
    sendKeys(sessionName, "C-a");
    sendKeys(sessionName, "q");
    sendKeys(sessionName, "a");
    sendKeys(sessionName, "Enter");
    TmuxDriver::sleepMs(600);

    // Verify backup file was created with original content
    TmuxDriver::sleepMs(400);
    std::ifstream backupFile(backupName.c_str());
    ASSERT_TRUE(backupFile.good()) << "Backup file was not created: " << backupName;

    std::string backupContents((std::istreambuf_iterator<char>(backupFile)),
                               std::istreambuf_iterator<char>());
    backupFile.close();

    EXPECT_EQ(backupContents, originalContent) << "Backup file content mismatch";

    // Verify modified file has new content
    std::ifstream modifiedFile(fileName.c_str());
    ASSERT_TRUE(modifiedFile.good());
    std::string modifiedContents((std::istreambuf_iterator<char>(modifiedFile)),
                                 std::istreambuf_iterator<char>());
    modifiedFile.close();

    EXPECT_NE(modifiedContents, originalContent) << "File was not modified";

    // Cleanup
    std::remove(fileName.c_str());
    std::remove(backupName.c_str());

    // Ensure tmux session is cleaned up
    killSession(sessionName);
}
