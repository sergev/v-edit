#pragma once

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "tempfile.h"
#include "workspace.h"

//
// Unified test fixture for Workspace unit tests.
// Provides Workspace instance with temporary file backing and helper methods
// for loading test files from disk.
//
class WorkspaceDriver : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<Tempfile> tempfile;
    std::unique_ptr<Workspace> wksp;

    //
    // Open a file from disk for reading by the workspace.
    // Returns file descriptor or throws if file cannot be opened.
    //
    int OpenFile(const std::string &path);
};
