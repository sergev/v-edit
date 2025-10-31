#include "TmuxDriver.h"

#include <unistd.h>

#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <thread>

// gtest already included in header

namespace {
std::string make_server_name()
{
    // Unique-ish per test process; avoids interfering with user's tmux.
    return std::string("v-edit-tests-") + std::to_string(getpid());
}
} // namespace

TmuxDriver::TmuxDriver() : server_name_(make_server_name())
{
}

TmuxDriver::~TmuxDriver()
{
    // Best-effort: kill all sessions on our server.
    run_system("tmux -L '" + server_name_ + "' kill-server > /dev/null 2>&1");
}

bool TmuxDriver::tmux_available() const
{
    int rc = std::system("tmux -V > /dev/null 2>&1");
    return rc == 0;
}

void TmuxDriver::SetUp()
{
    if (!tmux_available()) {
        GTEST_SKIP() << "tmux not found in PATH; skipping tmux-driven tests";
    }
}

void TmuxDriver::TearDown()
{
    // Ensure any leftover sessions are cleaned up between tests
    run_system("tmux -L '" + server_name_ + "' kill-server > /dev/null 2>&1");
}

void TmuxDriver::sleep_ms(int millis)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

std::string TmuxDriver::escape_single_quotes(const std::string &text)
{
    // Safely wrap in single quotes: ' -> '\'' pattern
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text) {
        if (c == '\'')
            out += "'\\''";
        else
            out.push_back(c);
    }
    return out;
}

std::string TmuxDriver::shell_quote(const std::string &text)
{
    return std::string("'") + escape_single_quotes(text) + "'";
}

std::string TmuxDriver::qualify(const std::string &sessionName) const
{
    // Namespace session by server name to avoid collisions across parallel runs
    return sessionName + std::string("-") + server_name_;
}

int TmuxDriver::run_system(const std::string &cmd) const
{
    return std::system(cmd.c_str());
}

std::string TmuxDriver::run_and_capture(const std::string &cmd) const
{
    std::array<char, 4096> buffer{};
    std::string result;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return result;
    while (true) {
        size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
        if (n == 0)
            break;
        result.append(buffer.data(), n);
    }
    pclose(pipe);
    return result;
}

void TmuxDriver::create_session(const std::string &sessionName, const std::string &command)
{
    // tmux -L <srv> new-session -d -s <name> '<cmd>'
    const std::string qname = qualify(sessionName);
    const std::string cmd   = "TERM=xterm tmux -L '" + server_name_ + "' new-session -d -s '" +
                            escape_single_quotes(qname) + "' " + command + " > /dev/null 2>&1";
    run_system(cmd);

    // Set a fixed window size for deterministic tests: 30 columns x 10 rows
    const std::string resize = "tmux -L '" + server_name_ + "' resize-window -t '" +
                               escape_single_quotes(qname) + "' -x 30 -y 10 > /dev/null 2>&1";
    run_system(resize);
}

void TmuxDriver::send_keys(const std::string &sessionName, const std::string &keys)
{
    // tmux -L <srv> send-keys -t <name> <keys>
    // Quote the keys using shell_quote to handle spaces properly
    const std::string qname = qualify(sessionName);
    const std::string cmd   = "TERM=xterm tmux -L '" + server_name_ + "' send-keys -t '" +
                            escape_single_quotes(qname) + "' " + shell_quote(keys) +
                            " > /dev/null 2>&1";
    run_system(cmd);
}

std::string TmuxDriver::capture_pane(const std::string &sessionName, int start)
{
    // tmux -L <srv> capture-pane -t <name> -pS <start>
    const std::string qname = qualify(sessionName);
    const std::string cmd   = "TERM=xterm tmux -L '" + server_name_ + "' capture-pane -t '" +
                            escape_single_quotes(qname) + "' -pS " + std::to_string(start) +
                            " 2>/dev/null";
    return run_and_capture(cmd);
}

void TmuxDriver::kill_session(const std::string &sessionName)
{
    const std::string qname = qualify(sessionName);
    const std::string cmd   = "TERM=xterm tmux -L '" + server_name_ + "' kill-session -t '" +
                            escape_single_quotes(qname) + "' > /dev/null 2>&1";
    run_system(cmd);
}

std::string TmuxDriver::capture_screen(const std::string &sessionName)
{
    // Capture exactly the visible screen using -p (no scrollback) and -J to join wrapped lines
    const std::string qname = qualify(sessionName);
    const std::string cmd   = "TERM=xterm tmux -L '" + server_name_ + "' capture-pane -t '" +
                            escape_single_quotes(qname) + "' -p -J 2>/dev/null";
    return run_and_capture(cmd);
}
