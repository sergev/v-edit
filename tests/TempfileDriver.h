#pragma once

#include <gtest/gtest.h>

#include <memory>

#include "tempfile.h"

class TempfileDriver : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<Tempfile> tempfile;
};
