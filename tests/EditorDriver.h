#pragma once

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "editor.h"

class EditorDriver : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<Editor> editor;

    // Initialize with a few blank lines so put_line() can update them.
    // put_line() only updates existing lines, doesn't create new ones.
    void CreateBlankLines(unsigned num_lines);

    // Helper: Create a line with content
    void CreateLine(int line_no, const std::string &content);

    // Helper: Load a line into current_line_ buffer
    void LoadLine(int line_no);

    // Helper: Get actual column position
    size_t GetActualCol() const;

    // File utilities for segment tests
    std::string createTestFile(const std::string &content);
    void cleanupTestFile(const std::string &filename);
};
