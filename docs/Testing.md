# Testing Methodology

## Overview

This project uses GoogleTest for unit tests and a lightweight tmux-driven fixture to exercise the ncurses application end-to-end in a controlled pseudo-terminal. The tmux-based approach allows us to:
- Launch the built `ve` binary in an isolated tmux session
- Send keystrokes programmatically
- Capture the visible pane output for assertions

All tests live under `tests/` and are built into the single test binary `v_edit_tests`.

## Test Build and Layout

- Top-level CMake adds the test subtree via `add_subdirectory(tests)`.
- `tests/CMakeLists.txt`:
  - Fetches GoogleTest via CMake FetchContent
  - Builds `v_edit_tests` from sources in `tests/`
  - Links `GTest::gtest_main` and `v_edit_lib`
  - Injects the path to the built app as `V_EDIT_BIN_PATH` (via compile definition)
  - Registers tests via `gtest_discover_tests`

Files of interest:
- `tests/TmuxDriver.h/.cpp` — GoogleTest fixture that wraps tmux commands
- `tests/smoke_tmux_test.cpp` — Example test using the fixture

## Running Tests

From the project root:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build
ctest -V --test-dir build/tests
```
Notes:
- Tests are skipped automatically if `tmux` is not available in `PATH`.
- Each test run uses a dedicated tmux server via the `-L` flag (e.g., `-L v-edit-tests-<pid>`) so it does not interfere with the user's tmux.

## Why tmux?

Ncurses apps require a terminal to drive input and render output. Spawning the app inside tmux gives us a reliable pseudo-terminal with:
- Scriptable input (`tmux send-keys`)
- Deterministic capture of what’s on screen (`tmux capture-pane`)
- Full lifecycle control over the process (`tmux new-session`/`kill-session`)

This makes it possible to write end-to-end tests that assert on what the user would see.

## The Tmux fixture

The fixture derives from `::testing::Test` and provides convenience helpers. It also handles setup/teardown and isolation of tmux resources per test run.

### Class
- `class TmuxDriver : public ::testing::Test`

### Lifecycle
- `void SetUp() override` — Skips the test (`GTEST_SKIP`) if tmux is not installed.
- `void TearDown() override` — Kills the dedicated tmux server to ensure a clean state between tests.

### Helpers
- `bool tmuxAvailable() const` — Quick check for `tmux` in `PATH`.
- `static void sleepMs(int millis)` — Simple sleep helper for short delays.
- `static std::string shellQuote(const std::string& text)` — Wraps a string with single quotes, escaping internal quotes for safe shell invocation.
- `void createSession(const std::string& session, const std::string& command)` — Creates a detached session and runs `command` inside it.
- `void sendKeys(const std::string& session, const std::string& keys)` — Sends keys to the session. Keys are passed as-is to `tmux send-keys` (e.g., `"C-c"`, `"q"`, `"Enter"`).
- `std::string capturePane(const std::string& session, int start = -100)` — Captures pane contents from `start` lines from the bottom.
- `void killSession(const std::string& session)` — Kills a specific session.

Internally, the fixture uses a unique tmux server name per process: `v-edit-tests-<pid>` via the `-L` flag for all commands.

## Adding a New Test

1) Create a new test file in `tests/`, e.g. `tests/my_feature_test.cpp`.
2) Include the fixture header and write a test with `TEST_F`:
```cpp
#include <gtest/gtest.h>
#include "TmuxDriver.h"

TEST_F(TmuxDriver, NavigatesAndRenders)
{
    const std::string session = "v-edit_nav";
    const std::string app     = V_EDIT_BIN_PATH;

    // Launch app
    createSession(session, shellQuote(app));
    TmuxDriver::sleepMs(200);

    // Drive some keys
    sendKeys(session, "j");
    sendKeys(session, "j");
    sendKeys(session, "k");
    TmuxDriver::sleepMs(100);

    // Inspect output
    const std::string pane = capturePane(session, -200);
    ASSERT_FALSE(pane.empty());
    // EXPECT_* assertions tailored to your feature

    // Cleanup
    killSession(session);
}
```
3) Add the file to the `v_edit_tests` target in `tests/CMakeLists.txt` (if not using globbing).
4) Rebuild and run `ctest -V --test-dir build/tests`.

## Tips and Pitfalls

- Timing: Ncurses output can be asynchronous. Use small sleeps (`sleepMs(50..200)`) between key sends and capture to stabilize.
- Keys: `tmux send-keys` accepts literal characters or named keys (e.g. `Enter`, `Escape`, or chords like `C-c`). Combine with spaces to send sequences.
- Output capture: `capturePane(session, -N)` captures the last N lines. Adjust `N` if your screen is larger/smaller.
- Cleanup: The fixture's teardown kills the tmux server, but it's still good practice to `killSession(session)` in the test after assertions.
- App path: Use `V_EDIT_BIN_PATH` (injected by CMake) to launch the built binary under test.
- Formatting: Run `make reindent` to apply `clang-format` consistently.

## Example: Minimal Smoke Test

See `tests/smoke_tmux_test.cpp` for a working example. It:
- Launches `ve` in tmux
- Waits briefly
- Captures the pane
- Asserts the output is non-empty and has printable characters

This forms a template for further end-to-end tests that validate specific UI behaviors.
