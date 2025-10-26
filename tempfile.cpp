#include "tempfile.h"

#include <unistd.h>

#include <cstring>

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
Segment *Tempfile::write_line_to_temp(const std::string &line_content)
{
    if (tempfile_fd_ < 0 && !open_temp_file()) {
        return nullptr;
    }

    std::string line = line_content;
    // Add newline if not present
    if (line.empty() || line.back() != '\n') {
        line += '\n';
    }

    long seek_pos = tempseek_;
    int nbytes    = line.size();
    if (write(tempfile_fd_, line.c_str(), nbytes) != nbytes) {
        return nullptr;
    }

    tempseek_ += nbytes;

    Segment *seg = new Segment();
    seg->prev    = nullptr;
    seg->next    = nullptr;
    seg->nlines  = 1;
    seg->fdesc   = tempfile_fd_;
    seg->seek    = seek_pos;
    seg->sizes.push_back(nbytes);

    return seg;
}

//
// Write line content and return the segment (for workspace internal use).
// Updates out_seek with the seek position.
//
Segment *Tempfile::write_line_internal(const std::string &line, long *out_seek)
{
    if (tempfile_fd_ < 0 && !open_temp_file()) {
        return nullptr;
    }

    std::string line_content = line;
    // Add newline if not present
    if (line_content.empty() || line_content.back() != '\n') {
        line_content += '\n';
    }

    long seek_pos = tempseek_;
    int nbytes    = line_content.size();
    if (write(tempfile_fd_, line_content.c_str(), nbytes) != nbytes) {
        return nullptr;
    }

    tempseek_ += nbytes;

    if (out_seek) {
        *out_seek = seek_pos;
    }

    Segment *seg = new Segment();
    seg->prev    = nullptr;
    seg->next    = nullptr;
    seg->nlines  = 1;
    seg->fdesc   = tempfile_fd_;
    seg->seek    = seek_pos;
    seg->sizes.push_back(nbytes);

    return seg;
}
