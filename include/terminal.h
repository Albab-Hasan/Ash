#ifndef ASH_TERMINAL_H
#define ASH_TERMINAL_H

#include <termios.h>
#include <unistd.h>

// Terminal control globals
extern pid_t shell_pgid;             // Our process group ID
extern int shell_terminal;           // Terminal file descriptor
extern struct termios shell_tmodes;  // Our terminal settings
extern int shell_is_interactive;     // Are we in interactive mode?

// Initialize terminal for interactive use
void terminal_init(void);

// Set up signal handlers for Ctrl+C, Ctrl+Z, etc.
void terminal_install_signal_handlers(void);

#endif
