#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <string>

#include "TmuxDriver.h"

TEST_F(TmuxDriver, DISABLED_VerticalScrollingShowsLaterLines)
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

    // Build expected content rows dynamically: top should be max(1, total-9) => 7 when total=15
    std::vector<std::string> expected;
    expected.reserve(10);
    const int total = 15;
    const int top   = std::max(1, total - 9);
    for (int i = top; i < top + 10; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "L%02d", i);
        expected.emplace_back(buf);
    }
    // Clip to L10..L15, rest are tildes
    for (auto &s : expected) {
        if (s > std::string("L15"))
            s = "~";
    }
    for (auto &l : expected)
        rtrim(l);

    // Debug: print expected lines
    std::cout << "--- Expected (10 lines) ---\n";
    for (size_t i = 0; i < expected.size(); ++i) {
        std::cout << i << ": '" << expected[i] << "'\n";
    }

    // Compare whole screen (content rows only)
    ASSERT_EQ(lines.size(), expected.size());
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
