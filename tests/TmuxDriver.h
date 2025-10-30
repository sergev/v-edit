#pragma once

#include <gtest/gtest.h>

#include <string>

// Minimal helper that drives tmux to run the built binary and interact with it.
// It isolates the tmux server via -L to avoid interfering with user sessions.
class TmuxDriver : public ::testing::Test {
public:
    TmuxDriver();
    ~TmuxDriver();

    bool tmuxAvailable() const;

protected:
    void SetUp() override;
    void TearDown() override;

    static void sleepMs(int millis);
    static std::string shellQuote(const std::string &text);

    void createSession(const std::string &sessionName, const std::string &command);
    void sendKeys(const std::string &sessionName, const std::string &keys);
    std::string capturePane(const std::string &sessionName, int start = -100);
    std::string captureScreen(const std::string &sessionName);
    void killSession(const std::string &sessionName);

private:
    std::string serverName;
    std::string qualify(const std::string &sessionName) const;

    static std::string escapeSingleQuotes(const std::string &text);
    int runSystem(const std::string &cmd) const;
    std::string runAndCapture(const std::string &cmd) const;
};
