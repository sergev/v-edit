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
std::string makeServerName()
{
    // Unique-ish per test process; avoids interfering with user's tmux.
    return std::string("v-edit-tests-") + std::to_string(getpid());
}
} // namespace

TmuxDriver::TmuxDriver() : serverName(makeServerName())
{
}

TmuxDriver::~TmuxDriver()
{
    // Best-effort: kill all sessions on our server.
    runSystem("tmux -L '" + serverName + "' kill-server > /dev/null 2>&1");
}

bool TmuxDriver::tmuxAvailable() const
{
    int rc = std::system("tmux -V > /dev/null 2>&1");
    return rc == 0;
}

void TmuxDriver::SetUp()
{
    if (!tmuxAvailable()) {
        GTEST_SKIP() << "tmux not found in PATH; skipping tmux-driven tests";
    }
}

void TmuxDriver::TearDown()
{
    // Ensure any leftover sessions are cleaned up between tests
    runSystem("tmux -L '" + serverName + "' kill-server > /dev/null 2>&1");
}

void TmuxDriver::sleepMs(int millis)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

std::string TmuxDriver::escapeSingleQuotes(const std::string &text)
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

std::string TmuxDriver::shellQuote(const std::string &text)
{
    return std::string("'") + escapeSingleQuotes(text) + "'";
}

int TmuxDriver::runSystem(const std::string &cmd) const
{
    return std::system(cmd.c_str());
}

std::string TmuxDriver::runAndCapture(const std::string &cmd) const
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

void TmuxDriver::createSession(const std::string &sessionName, const std::string &command)
{
    // tmux -L <srv> new-session -d -s <name> '<cmd>'
    const std::string cmd = "TERM=xterm tmux -L '" + serverName + "' new-session -d -s '" +
                            escapeSingleQuotes(sessionName) + "' " + command + " > /dev/null 2>&1";
    runSystem(cmd);

    // Set a fixed window size for deterministic tests: 30 columns x 10 rows
    const std::string resize = "tmux -L '" + serverName + "' resize-window -t '" +
                               escapeSingleQuotes(sessionName) + "' -x 30 -y 10 > /dev/null 2>&1";
    runSystem(resize);
}

void TmuxDriver::sendKeys(const std::string &sessionName, const std::string &keys)
{
    // tmux -L <srv> send-keys -t <name> <keys>
    // Quote the keys using shellQuote to handle spaces properly
    const std::string cmd = "TERM=xterm tmux -L '" + serverName + "' send-keys -t '" +
                            escapeSingleQuotes(sessionName) + "' " + shellQuote(keys) +
                            " > /dev/null 2>&1";
    runSystem(cmd);
}

std::string TmuxDriver::capturePane(const std::string &sessionName, int start)
{
    // tmux -L <srv> capture-pane -t <name> -pS <start>
    const std::string cmd = "TERM=xterm tmux -L '" + serverName + "' capture-pane -t '" +
                            escapeSingleQuotes(sessionName) + "' -pS " + std::to_string(start) +
                            " 2>/dev/null";
    return runAndCapture(cmd);
}

void TmuxDriver::killSession(const std::string &sessionName)
{
    const std::string cmd = "TERM=xterm tmux -L '" + serverName + "' kill-session -t '" +
                            escapeSingleQuotes(sessionName) + "' > /dev/null 2>&1";
    runSystem(cmd);
}

std::string TmuxDriver::captureScreen(const std::string &sessionName)
{
    // Capture exactly the visible screen using -p (no scrollback) and -J to join wrapped lines
    const std::string cmd = "TERM=xterm tmux -L '" + serverName + "' capture-pane -t '" +
                            escapeSingleQuotes(sessionName) + "' -p -J 2>/dev/null";
    return runAndCapture(cmd);
}
