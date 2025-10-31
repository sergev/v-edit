#pragma once

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "tempfile.h"
#include "workspace.h"

class WorkspaceDriver : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<Tempfile> tempfile;
    std::unique_ptr<Workspace> wksp;

    int OpenFile(const std::string &path);
};
