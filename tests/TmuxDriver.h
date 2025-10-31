#pragma once

#include <gtest/gtest.h>

#include <string>

// Minimal helper that drives tmux to run the built binary and interact with it.
// It isolates the tmux server via -L to avoid interfering with user sessions.
class TmuxDriver : public ::testing::Test {
public:
    TmuxDriver();
    ~TmuxDriver();

    bool tmux_available() const;

protected:
    void SetUp() override;
    void TearDown() override;

    static void sleep_ms(int millis);
    static std::string shell_quote(const std::string &text);

    void create_session(const std::string &sessionName, const std::string &command);
    void send_keys(const std::string &sessionName, const std::string &keys);
    std::string capture_pane(const std::string &sessionName, int start = -100);
    std::string capture_screen(const std::string &sessionName);
    void kill_session(const std::string &sessionName);

private:
    std::string server_name_;
    std::string qualify(const std::string &sessionName) const;

    static std::string escape_single_quotes(const std::string &text);
    int run_system(const std::string &cmd) const;
    std::string run_and_capture(const std::string &cmd) const;
};
