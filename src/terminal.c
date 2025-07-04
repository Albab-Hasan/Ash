#include "terminal.h"
#include <signal.h>
#include <stdio.h>
#include <readline/readline.h>
#include <unistd.h>
#include <errno.h>

/* ------------------------------------------------------------------------- */
/*  Globals (definitions)                                                     */
/* ------------------------------------------------------------------------- */

pid_t shell_pgid = 0;
int shell_terminal = 0;
struct termios shell_tmodes;
int shell_is_interactive = 0;

/* ------------------------------------------------------------------------- */
static void handle_sigint(int sig)
{
  (void)sig;
  /* Print newline then let readline redisplay prompt cleanly */
  printf("\n");
  rl_on_new_line();
  rl_redisplay();
}

static void handle_sigtstp(int sig)
{
  (void)sig;
  printf("\n");
  rl_on_new_line();
  rl_redisplay();
}

/* ------------------------------------------------------------------------- */
void terminal_install_signal_handlers(void)
{
  signal(SIGINT, handle_sigint);
  signal(SIGTSTP, handle_sigtstp);
}

/* ------------------------------------------------------------------------- */
void terminal_init(void)
{
  /* Determine if we are running interactively */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);

  if (!shell_is_interactive)
    return;

  /* Loop until we are in the foreground */
  while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
  {
    kill(-shell_pgid, SIGTTIN);
  }

  /* Ignore signals that interactive shells usually ignore */
  signal(SIGINT, SIG_IGN);  /* Ctrl-C */
  signal(SIGQUIT, SIG_IGN); /* Ctrl-\ */
  signal(SIGTSTP, SIG_IGN); /* Ctrl-Z */
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  /* Put ourselves in our own process group */
  shell_pgid = getpid();
  if (setpgid(shell_pgid, shell_pgid) < 0)
  {
    perror("ash: couldn't put the shell in its own process group");
    _exit(1);
  }

  /* Grab control of the terminal */
  tcsetpgrp(shell_terminal, shell_pgid);

  /* Save default terminal attributes */
  tcgetattr(shell_terminal, &shell_tmodes);
}
