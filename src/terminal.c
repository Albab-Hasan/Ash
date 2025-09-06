#include "terminal.h"
#include <signal.h>
#include <stdio.h>
#include <readline/readline.h>
#include <unistd.h>
#include <errno.h>

// Terminal control globals
pid_t shell_pgid = 0;
int shell_terminal = 0;
struct termios shell_tmodes;
int shell_is_interactive = 0;

// Handle Ctrl+C by showing a new prompt
static void handle_sigint(int sig) {
  (void)sig;  // Unused

  // Print a newline and redisplay prompt
  printf("\n");
  rl_on_new_line();
  rl_redisplay();
}

// Handle Ctrl+Z similarly
static void handle_sigtstp(int sig) {
  (void)sig;  // Unused

  printf("\n");
  rl_on_new_line();
  rl_redisplay();
}

// Set up our signal handlers for interactive use
void terminal_install_signal_handlers(void) {
  signal(SIGINT, handle_sigint);    // Ctrl+C
  signal(SIGTSTP, handle_sigtstp);  // Ctrl+Z
}

// Initialize terminal for interactive use
void terminal_init(void) {
  // Check if we're connected to a terminal
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);

  if (!shell_is_interactive) return;

  // Make sure we're in the foreground
  while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) {
    // If not, wait until we are
    kill(-shell_pgid, SIGTTIN);
  }

  // Ignore job control signals for now
  signal(SIGINT, SIG_IGN);   // Ctrl+C
  signal(SIGQUIT, SIG_IGN);  // Ctrl+backslash
  signal(SIGTSTP, SIG_IGN); // Ctrl+Z
  signal(SIGTTIN, SIG_IGN);  // Terminal read from bg
  signal(SIGTTOU, SIG_IGN);  // Terminal write from bg

  // Create our own process group
  shell_pgid = getpid();
  if (setpgid(shell_pgid, shell_pgid) < 0) {
    perror("ash: couldn't put the shell in its own process group");
    _exit(1);
  }

  // Take control of the terminal
  tcsetpgrp(shell_terminal, shell_pgid);

  // Save our terminal settings
  tcgetattr(shell_terminal, &shell_tmodes);
}
