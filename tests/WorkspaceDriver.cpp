#include "WorkspaceDriver.h"

void WorkspaceDriver::SetUp()
{
    tempfile = std::make_unique<Tempfile>();
    wksp     = std::make_unique<Workspace>(*tempfile);
    // Open temp file for workspace use
    tempfile->open_temp_file();
}

void WorkspaceDriver::TearDown()
{
    wksp.reset();
    tempfile.reset();
}

int WorkspaceDriver::OpenFile(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    return fd;
}
