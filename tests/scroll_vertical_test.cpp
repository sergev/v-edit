#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <string>

#include "TmuxDriver.h"

TEST_F(TmuxDriver, VerticalScrollingShowsLaterLines)
{
    const std::string sessionName = "v-edit_scroll_vert";
    const std::string appPath     = V_EDIT_BIN_PATH;

    const std::string fileName =
        std::string("v-edit_scroll_test_") + std::to_string(getpid()) + ".txt";
    std::remove(fileName.c_str());

    // Launch editor with temp file in current directory (build/tests)
    createSession(sessionName, shellQuote(appPath + std::string(" ") + fileName));
    TmuxDriver::sleepMs(200);

    // Insert more lines than fit in 9-row content area (window is 10 rows incl. status)
    for (int i = 1; i <= 15; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "L%02d", i);
        sendKeys(sessionName, buf);
        if (i < 15)
            sendKeys(sessionName, "Enter");
        TmuxDriver::sleepMs(10);
    }

    TmuxDriver::sleepMs(200);
    std::string paneAll = captureScreen(sessionName);
    if (paneAll.empty()) {
        TmuxDriver::sleepMs(300);
        paneAll = capturePane(sessionName, -50);
    }
    if (paneAll.empty()) {
        // fall back to larger capture window
        paneAll = capturePane(sessionName, -500);
    }
    ASSERT_FALSE(paneAll.empty());

    // Extract the last 10 lines (full screen) and trim trailing spaces per line
    std::vector<std::string> lines;
    {
        std::string acc;
        lines.reserve(16);
        for (char c : paneAll) {
            if (c == '\r')
                continue;
            if (c == '\n') {
                lines.push_back(acc);
                acc.clear();
            } else
                acc.push_back(c);
        }
        if (!acc.empty())
            lines.push_back(acc);
        if (lines.size() >= 10) {
            lines.erase(lines.begin(), lines.end() - 10);
        }
    }
    auto rtrim = [](std::string &s) {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
    };
    for (auto &l : lines)
        rtrim(l);

    ASSERT_EQ(lines.size(), 10u);

    // Debug: print captured raw pane and normalized lines
    std::cout << "--- Captured (raw) ---\n" << paneAll << "\n";
    std::cout << "--- Captured (normalized last 10) ---\n";
    for (size_t i = 0; i < lines.size(); ++i) {
        std::cout << i << ": '" << lines[i] << "'\n";
    }

    // Build expected content rows dynamically: for total=15 lines with 9 visible rows,
    // we expect to show the last 9 lines (L07-L15)
    // The window has 10 total rows (9 content + 1 status)
    std::vector<std::string> expected;
    expected.reserve(10);
    const int total = 15;
    const int visible_rows = 9; // 9 content rows in a 10-row window
    const int top   = std::max(1, total - visible_rows + 1); // top = 15 - 9 + 1 = 7
    for (int i = top; i <= total; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "L%02d", i);
        expected.emplace_back(buf);
    }
    for (auto &l : expected)
        rtrim(l);

    // Debug: print expected lines
    std::cout << "--- Expected (" << expected.size() << " content lines) ---\n";
    for (size_t i = 0; i < expected.size(); ++i) {
        std::cout << i << ": '" << expected[i] << "'\n";
    }

    // Compare content rows only (excluding status line which is last)
    ASSERT_GE(lines.size(), expected.size()) << "Not enough captured lines";
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(lines[i], expected[i])
            << "Mismatch at row " << i << " got='" << lines[i] << "' exp='" << expected[i] << "'";
    }

    // Exit and cleanup
    sendKeys(sessionName, "C-a");
    sendKeys(sessionName, "q");
    sendKeys(sessionName, "a");
    sendKeys(sessionName, "Enter");
    TmuxDriver::sleepMs(300);
    std::remove(fileName.c_str());
    killSession(sessionName);
}
