#include "tempfile.h"

#include <unistd.h>

#include <cstring>
#include <list>
#include <vector>

Tempfile::Tempfile() = default;

Tempfile::~Tempfile()
{
    close_temp_file();
}

//
// Open temporary file for storing modified lines.
//
bool Tempfile::open_temp_file()
{
    if (tempfile_fd_ >= 0) {
        return true; // Already open
    }

    char template_name[] = "/tmp/v-edit-XXXXXX";
    tempfile_fd_         = mkstemp(template_name);
    if (tempfile_fd_ < 0) {
        return false;
    }

    // Unlink immediately so file is deleted when closed
    unlink(template_name);
    tempseek_ = 0;
    return true;
}

//
// Close temporary file.
//
void Tempfile::close_temp_file()
{
    if (tempfile_fd_ >= 0) {
        close(tempfile_fd_);
        tempfile_fd_ = -1;
        tempseek_    = 0;
    }
}

//
// Write a line to the temporary file and return a segment for it.
//
std::list<Segment> Tempfile::write_line_to_temp(const std::string &line_content)
{
    if (tempfile_fd_ < 0 && !open_temp_file()) {
        return {};
    }

    std::string line = line_content;
    // Add newline if not present
    if (line.empty() || line.back() != '\n') {
        line += '\n';
    }

    long seek_pos = tempseek_;
    int nbytes    = line.size();

    // Seek to the correct position before writing
    if (lseek(tempfile_fd_, seek_pos, SEEK_SET) < 0) {
        return {};
    }

    if (write(tempfile_fd_, line.c_str(), nbytes) != nbytes) {
        return {};
    }

    tempseek_ += nbytes;

    Segment seg;
    seg.line_count = 1;
    seg.fdesc  = tempfile_fd_;
    seg.seek   = seek_pos;
    seg.sizes.push_back(nbytes);

    return { seg };
}

//
// Write multiple lines to temporary file and return a segment for them.
//
std::list<Segment> Tempfile::write_lines_to_temp(const std::vector<std::string> &lines)
{
    if (tempfile_fd_ < 0 && !open_temp_file()) {
        return {};
    }

    if (lines.empty()) {
        return {};
    }

    // Write all lines to temp file
    Segment seg;
    seg.line_count = lines.size();
    seg.fdesc  = tempfile_fd_;
    seg.seek   = tempseek_;

    // Seek to the correct position before writing
    if (lseek(tempfile_fd_, tempseek_, SEEK_SET) < 0) {
        return {};
    }

    // Write lines and record their sizes
    for (const std::string &ln : lines) {
        std::string line = ln;
        // Add newline if not present
        if (line.empty() || line.back() != '\n') {
            line += '\n';
        }

        int nbytes = line.size();
        if (write(tempfile_fd_, line.c_str(), nbytes) != nbytes) {
            return {};
        }

        seg.sizes.push_back(nbytes);
        tempseek_ += nbytes;
    }

    return { seg };
}
