#include <getopt.h>
#include <cstring>
#include <iostream>

#include "editor.h"

//
// Display editor usage information.
//
static void print_usage(const char *progname)
{
    std::cout << "ve - A terminal-based text editor" << std::endl;
    std::cout << "Version: 0.1.0" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << progname << " [OPTIONS] [file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -v, --version  Show version information" << std::endl;
    std::cout << "  -r, --replay   Replay last session from journal" << std::endl;
    std::cout << "  (no args)      Restore last session" << std::endl;
    std::cout << std::endl;
    std::cout << "Keys:" << std::endl;
    std::cout << "  ^A or F1  Enter command mode" << std::endl;
    std::cout << "  ^A q      Save and quit" << std::endl;
    std::cout << "  ^A qa     Quit without save" << std::endl;
    std::cout << "  F2        Save file" << std::endl;
    std::cout << "  F3        Show help, switch workspaces" << std::endl;
}

int main(int argc, char *argv[])
{
    // Define long options
    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { "replay", no_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    int restart = 0;
    bool replay_flag = false;

    // Parse options
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hvr", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 'v':
            std::cout << "ve version 0.1.0" << std::endl;
            return 0;
        case 'r':
            replay_flag = true;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // Determine restart mode
    if (replay_flag) {
        restart = 2; // replay
    } else if (optind == 1 && argc == 1) {
        // No arguments at all (original argc == 1, nothing was parsed)
        restart = 1; // restore attempt
    } else {
        restart = 0; // normal
    }

    // Adjust argc and argv to point to remaining non-option arguments
    // Shift so argv[1] contains filename if present
    int new_argc = argc - optind + 1;
    char **new_argv = argv + optind - 1;
    new_argv[0] = argv[0]; // Keep program name

    Editor editor;
    return editor.run(restart, new_argc, new_argv);
}
