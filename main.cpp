#include <cstring>
#include <iostream>

#include "editor.h"

static void print_usage(const char *progname)
{
    std::cout << "ve - A terminal-based text editor" << std::endl;
    std::cout << "Version: 0.1.0" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << progname << " [OPTIONS] [file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help, -h     Show this help message" << std::endl;
    std::cout << "  --version, -v  Show version information" << std::endl;
    std::cout << "  -              Replay last session from journal" << std::endl;
    std::cout << "  (no args)      Restore last session" << std::endl;
    std::cout << std::endl;
    std::cout << "Keys:" << std::endl;
    std::cout << "  ^A        Enter command mode" << std::endl;
    std::cout << "  ^A qa     Quit without save" << std::endl;
    std::cout << "  /text     Search forward" << std::endl;
    std::cout << "  ?text     Search backward" << std::endl;
    std::cout << "  n         Repeat search" << std::endl;
    std::cout << "  F2        Save file" << std::endl;
}

int main(int argc, char *argv[])
{
    // Parse options first
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::cout << "ve version 0.1.0" << std::endl;
            return 0;
        }
    }

    Editor editor;
    return editor.run(argc, argv);
}
