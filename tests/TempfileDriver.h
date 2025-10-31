#pragma once

#include <gtest/gtest.h>

#include <memory>

#include "tempfile.h"

//
// Unified test fixture for Tempfile unit tests.
// Provides Tempfile instance for testing temporary file creation, management,
// and on-demand line loading from disk.
//
class TempfileDriver : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<Tempfile> tempfile;
};
