#ifndef ASH_TERMINAL_H
#define ASH_TERMINAL_H

#include <termios.h>
#include <unistd.h>

/* Exposed terminal/job-control globals */
extern pid_t shell_pgid;            /* shell's process-group id */
extern int shell_terminal;          /* controlling tty fd (usually STDIN_FILENO) */
extern struct termios shell_tmodes; /* saved tty modes */
extern int shell_is_interactive;    /* boolean */

/* Initialises interactive mode (process group, tty, ignores) */
void terminal_init(void);

/* Install SIGINT / SIGTSTP handlers suitable for readline */
void terminal_install_signal_handlers(void);

#endif
