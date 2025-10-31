#include "TempfileDriver.h"

void TempfileDriver::SetUp()
{
    tempfile = std::make_unique<Tempfile>();
}

void TempfileDriver::TearDown()
{
    tempfile.reset();
}
