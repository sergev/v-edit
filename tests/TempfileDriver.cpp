#include "TempfileDriver.h"

void TempfileDriver::SetUp()
{
    // Create temp file instance without opening
    tempfile = std::make_unique<Tempfile>();
}

void TempfileDriver::TearDown()
{
    tempfile.reset();
}
