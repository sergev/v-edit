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
    create_session(sessionName, shell_quote(appPath + std::string(" ") + filePath));
    TmuxDriver::sleep_ms(250);

    // Type lines: "hello", newline, "world", newline, "!"
    send_keys(sessionName, "hello");
    send_keys(sessionName, "Enter");
    send_keys(sessionName, "Down");
    send_keys(sessionName, "Down");
    send_keys(sessionName, "world");
    send_keys(sessionName, "Enter");
    send_keys(sessionName, "!");
    TmuxDriver::sleep_ms(100);

    // Save via F2
    send_keys(sessionName, "F2");
    TmuxDriver::sleep_ms(200);

    // Exit via ^A, qa, Enter
    send_keys(sessionName, "C-a");
    send_keys(sessionName, "q");
    send_keys(sessionName, "a");
    send_keys(sessionName, "Enter");
    TmuxDriver::sleep_ms(600);

    // Verify file contents (allow a short delay for filesystem sync)
    TmuxDriver::sleep_ms(400);
    std::ifstream in(filePath.c_str());
    ASSERT_TRUE(in.good()) << "File was not created: " << filePath;
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // Note: newline is appended by editor.
    EXPECT_EQ(contents, std::string("hello\n\n\nworld\n!\n"));

    // Cleanup
    std::remove(filePath.c_str());

    // Ensure tmux session is cleaned up in any case
    kill_session(sessionName);
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
    create_session(sessionName, shell_quote(appPath + std::string(" ") + fileName));
    TmuxDriver::sleep_ms(250);

    // Make an edit - move to end and add a line
    send_keys(sessionName, "End");
    send_keys(sessionName, "Enter");
    send_keys(sessionName, "modified line");
    TmuxDriver::sleep_ms(100);

    // Save via F2 - this should create backup
    send_keys(sessionName, "F2");
    TmuxDriver::sleep_ms(200);

    // Exit via ^A, qa, Enter
    send_keys(sessionName, "C-a");
    send_keys(sessionName, "q");
    send_keys(sessionName, "a");
    send_keys(sessionName, "Enter");
    TmuxDriver::sleep_ms(600);

    // Verify backup file was created with original content
    TmuxDriver::sleep_ms(400);
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
    kill_session(sessionName);
}
