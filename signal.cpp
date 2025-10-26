#include <ncurses.h>
#include <signal.h>

#include <cstdlib>
#include <iostream>

#include "editor.h"

// Static instance pointer for signal handlers
Editor *Editor::instance_ = nullptr;

//
// Register signal handlers for graceful shutdown.
//
void Editor::setup_signal_handlers()
{
    // Store instance pointer for signal handlers
    instance_ = this;

    // Set up signal handlers
    // SIGTERM and other fatal signals
    signal(SIGTERM, handle_fatal_signal);
    signal(SIGQUIT, handle_fatal_signal);
    signal(SIGSEGV, handle_fatal_signal);
    signal(SIGABRT, handle_fatal_signal);
    signal(SIGFPE, handle_fatal_signal);
    signal(SIGILL, handle_fatal_signal);
    signal(SIGBUS, handle_fatal_signal);

    // SIGINT - interrupt (can be handled gracefully)
    signal(SIGINT, handle_sigint);
}

//
// Handle fatal signals by saving state before exit.
//
void Editor::handle_fatal_signal(int sig)
{
    // Close ncurses cleanly
    if (instance_) {
        endwin();
    }

    // Print error message
    std::cerr << "\nFirst the bad news: editor just died from signal " << sig << "\n";
    std::cerr << "Now the good news - your editing session may be preserved.\n";
    std::cerr << "Check ~/.ve/session for recovery.\n";

    // Exit
    std::exit(1);
}

//
// Handle interrupt signal to allow graceful cancel.
//
void Editor::handle_sigint(int sig)
{
    if (instance_ && instance_->interrupt_flag) {
        // Second SIGINT - abort
        if (instance_) {
            endwin();
        }
        std::cerr << "\nV-EDIT WAS INTERRUPTED\n";
        std::exit(1);
    }

    // First SIGINT - set flag
    if (instance_) {
        instance_->interrupt_flag = true;
    }

    // Reinstall handler for next time
    signal(SIGINT, handle_sigint);
}

//
// Process interrupt flag if set.
//
void Editor::check_interrupt()
{
    if (interrupt_flag) {
        interrupt_flag = false;
        status         = "Interrupt";
    }
}
