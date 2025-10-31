#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TmuxDriver.h"

namespace fs = std::filesystem;

TEST_F(TmuxDriver, RectangularBlockCopyPaste)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create a test file with structured content
    std::ofstream f(testFile);
    f << "Line 1: abc123def456\n";
    f << "Line 2: ghi789jkl012\n";
    f << "Line 3: mno345pqr678\n";
    f << "Line 4: stu901vwx234\n";
    f.close();

    // Start editor with the test file
    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Move cursor to position (line 0, column 8) - before "123"
    for (int i = 0; i < 8; ++i) {
        send_keys(session, "Right");
        TmuxDriver::sleep_ms(50);
    }

    // Enter command mode
    send_keys(session, "F1");
    TmuxDriver::sleep_ms(200);

    // Move cursor to define rectangular area (down 2 lines, right 3 characters)
    send_keys(session, "Down");
    TmuxDriver::sleep_ms(50);
    send_keys(session, "Down");
    TmuxDriver::sleep_ms(50);
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(50);
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(50);
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(50);

    // Copy the rectangular block with ^C (this also terminates selection)
    send_keys(session, "C-c");
    TmuxDriver::sleep_ms(200);

    // Move to a different location
    for (int i = 0; i < 10; ++i) {
        send_keys(session, "Right");
        TmuxDriver::sleep_ms(50);
    }

    // Paste the rectangular block (^V)
    send_keys(session, "C-v");
    TmuxDriver::sleep_ms(200);

    // Exit and save
    send_keys(session, "C-a");
    send_keys(session, "q");
    send_keys(session, "a");
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(600);

    // Verify the file content
    std::ifstream in(testFile);
    ASSERT_TRUE(in.good());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // The file should have been modified
    EXPECT_GT(contents.length(), 0);

    // Cleanup
    fs::remove(testFile);
    kill_session(session);
}

TEST_F(TmuxDriver, RectangularBlockDelete)
{
    const std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string session  = testName;
    const std::string app      = V_EDIT_BIN_PATH;
    const std::string testFile = testName + ".txt";

    // Create a test file with structured content
    std::ofstream f(testFile);
    f << "Line 1: abc123def456\n";
    f << "Line 2: ghi789jkl012\n";
    f << "Line 3: mno345pqr678\n";
    f << "Line 4: stu901vwx234\n";
    f.close();

    // Start editor with the test file
    create_session(session, shell_quote(app) + " " + shell_quote(testFile));
    TmuxDriver::sleep_ms(300);

    // Move cursor to position (line 0, column 8)
    for (int i = 0; i < 8; ++i) {
        send_keys(session, "Right");
        TmuxDriver::sleep_ms(50);
    }

    // Enter command mode
    send_keys(session, "F1");
    TmuxDriver::sleep_ms(200);

    // Move cursor to define rectangular area
    send_keys(session, "Down");
    TmuxDriver::sleep_ms(50);
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(50);
    send_keys(session, "Right");
    TmuxDriver::sleep_ms(50);

    // Delete the rectangular block with ^Y (this also terminates selection)
    send_keys(session, "C-y");
    TmuxDriver::sleep_ms(200);

    // Exit and save
    send_keys(session, "C-a");
    send_keys(session, "q");
    send_keys(session, "a");
    send_keys(session, "Enter");
    TmuxDriver::sleep_ms(600);

    // Verify the file content
    std::ifstream in(testFile);
    ASSERT_TRUE(in.good());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    // The file should have been modified
    EXPECT_GT(contents.length(), 0);

    // Cleanup
    fs::remove(testFile);
    kill_session(session);
}
