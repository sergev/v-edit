#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>

#include "editor.h"
#include "segment.h"

//
// Load file content into segment chain structure.
//
bool Editor::load_file_to_segments(const std::string &path)
{
    wksp.load_file_to_segments(path);
    if (!wksp.chain()) {
        status = std::string("Cannot open file: ") + path;
        return false;
    }
    return true;
}
