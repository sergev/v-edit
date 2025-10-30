#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
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

    TmuxDriver::sleepMs(400);
    // Force a redraw via command mode 'r' to ensure pane has content
    sendKeys(sessionName, "F1");
    TmuxDriver::sleepMs(150);
    sendKeys(sessionName, "r");
    TmuxDriver::sleepMs(100);
    sendKeys(sessionName, "Enter");
    TmuxDriver::sleepMs(200);
    std::string paneAll = capturePane(sessionName, -500);
    if (paneAll.empty()) {
        TmuxDriver::sleepMs(300);
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
    const int total        = 15;
    const int visible_rows = 9; // 9 content rows in a 10-row window
    const int top          = std::max(1, total - visible_rows + 1); // top = 15 - 9 + 1 = 7
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

TEST_F(TmuxDriver, PageDownScrollingAndVirtualPositions)
{
    const std::string sessionName = "v-edit_pagedown";
    const std::string appPath     = V_EDIT_BIN_PATH;

    // Create a file with 20 lines (numbered 0-19)
    const std::string fileName =
        std::string("v-edit_pagedown_test_") + std::to_string(getpid()) + ".txt";
    std::remove(fileName.c_str());
    std::ofstream f(fileName);
    for (int i = 0; i < 20; i++) {
        f << "Line " << i << "\n";
    }
    f.close();

    // Launch editor with the file
    createSession(sessionName, shellQuote(appPath + std::string(" ") + fileName));
    TmuxDriver::sleepMs(300);

    // Test 1: Page Down within the file should show later lines
    // Press Page Down a few times to scroll through the file
    sendKeys(sessionName, "NPage");
    TmuxDriver::sleepMs(200);
    sendKeys(sessionName, "NPage");
    TmuxDriver::sleepMs(200);

    std::string pane1 = capturePane(sessionName, -10);
    // Should see some lines from the middle/end of the file
    bool foundLaterLine = false;
    for (int i = 8; i < 20; i++) {
        std::ostringstream oss;
        oss << "Line " << i;
        if (pane1.find(oss.str()) != std::string::npos) {
            foundLaterLine = true;
            break;
        }
    }
    EXPECT_TRUE(foundLaterLine) << "Page Down should show later lines. Pane: " << pane1;

    // Test 2: Page Down can scroll beyond file end (virtual positions)
    // Continue pressing Page Down until we scroll beyond the 20-line file
    // With ~8 lines visible per page, we need about 20/8 = 2.5 pages to reach the end
    // Then a few more to go beyond
    for (int i = 0; i < 5; i++) {
        sendKeys(sessionName, "NPage");
        TmuxDriver::sleepMs(200);
    }

    std::string pane2 = capturePane(sessionName, -10);

    // Should see virtual lines (marked with ~) when beyond file end
    // Check if we're showing virtual lines
    bool hasVirtualLines = (pane2.find('~') != std::string::npos);

    // Check status line - should show a line number >= 20 (beyond file end)
    bool beyondFileEnd = false;
    std::istringstream statusStream(pane2);
    std::string line;
    while (std::getline(statusStream, line)) {
        size_t linePos = line.find("Line=");
        if (linePos != std::string::npos) {
            size_t numStart = linePos + 5; // "Line=" is 5 chars
            size_t numEnd   = line.find_first_not_of("0123456789", numStart);
            if (numEnd != std::string::npos) {
                std::string lineNumStr = line.substr(numStart, numEnd - numStart);
                int lineNum            = std::stoi(lineNumStr);
                if (lineNum >= 20) {
                    beyondFileEnd = true;
                    break;
                }
            }
        }
    }

    // Verify we can scroll beyond file end
    EXPECT_TRUE(beyondFileEnd || hasVirtualLines)
        << "Page Down should allow scrolling beyond file end. Pane: " << pane2;

    // Test 3: Page Up should bring us back into the file
    // Press Page Up multiple times to scroll back into the file content
    // We scrolled 7 pages total (2 initially + 5 more), so need to scroll back enough
    for (int i = 0; i < 6; i++) {
        sendKeys(sessionName, "PPage");
        TmuxDriver::sleepMs(200);
    }

    std::string pane3 = capturePane(sessionName, -10);
    // Should see actual content lines again
    bool foundContentLine = false;
    for (int i = 0; i < 20; i++) {
        std::ostringstream oss;
        oss << "Line " << i;
        if (pane3.find(oss.str()) != std::string::npos) {
            foundContentLine = true;
            break;
        }
    }
    EXPECT_TRUE(foundContentLine) << "Page Up should bring us back to actual content. Pane: "
                                  << pane3;

    // Exit and cleanup
    sendKeys(sessionName, "C-a");
    sendKeys(sessionName, "q");
    sendKeys(sessionName, "a");
    sendKeys(sessionName, "Enter");
    TmuxDriver::sleepMs(300);
    std::remove(fileName.c_str());
    killSession(sessionName);
}
