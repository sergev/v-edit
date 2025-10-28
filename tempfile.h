#ifndef TEMPFILE_H
#define TEMPFILE_H

#include <list>
#include <string>

#include "segment.h"

//
// Tempfile class - manages temporary file for storing modified lines.
// Shared by all workspaces in an editor instance.
//
class Tempfile {
public:
    Tempfile();
    ~Tempfile();

    // No copying
    Tempfile(const Tempfile &)            = delete;
    Tempfile &operator=(const Tempfile &) = delete;

    // Open temporary file for storing modified lines
    bool open_temp_file();

    // Close temporary file
    void close_temp_file();

    // Write a line to the temporary file and return a segment for it
    std::list<Segment> write_line_to_temp(const std::string &line_content);

    // Write multiple lines to temporary file and return a segment for them
    std::list<Segment> write_lines_to_temp(const std::vector<std::string> &lines);

    // Get current file descriptor
    int fd() const { return tempfile_fd_; }

private:
    int tempfile_fd_{ -1 }; // file descriptor for temporary file
    long tempseek_{ 0 };    // seek position for temporary file
};

#endif // TEMPFILE_H
