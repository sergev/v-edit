#pragma once

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "editor.h"

//
// Unified test fixture for all Editor-based unit tests.
// Provides Editor instance with workspace, view state, and helper methods
// for creating/loading lines and managing test files.
//
class EditorDriver : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<Editor> editor;

    //
    // Create blank lines at the start of the workspace.
    // Useful for tests that need to use put_line() which only updates existing lines.
    //
    void CreateBlankLines(unsigned num_lines);

    //
    // Create or update a line with content at the specified line number.
    // Extends the workspace with blank lines if needed to reach the target line.
    //
    void CreateLine(int line_no, const std::string &content);

    //
    // Load a line from the workspace into the current_line_ buffer for editing.
    // Call this before modifying a line with put_line().
    //
    void LoadLine(int line_no);

    //
    // Get the actual column position accounting for horizontal scrolling.
    // Returns basecol + cursor_col for testing editing operations with scroll.
    //
    size_t GetActualCol() const;

    //
    // Create a temporary file with the given content and return its filename.
    // Filename is based on the test name to avoid collisions.
    //
    std::string createTestFile(const std::string &content);

    //
    // Remove a temporary test file created by createTestFile().
    //
    void cleanupTestFile(const std::string &filename);
};
