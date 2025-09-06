/**
 * shell.c - My custom shell implementation
 *
 * This is a simple Unix-like shell I'm building to learn more about
 * process management and system calls. It supports basic features like:
 * - Running commands
 * - Built-ins (cd, exit, history)
 * - Background processes with &
 * - I/O redirection
 * - Pipes
 * - Job control
 */
/* ash - minimal Unix-like shell */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <termios.h>   /* Terminal control */
#include <sys/ioctl.h> /* ioctl for terminal control */
#include <ctype.h>
#include "vars.h"
#include "shell.h"
#include "parser.h"
#include "tokenizer.h"
#include "builtins.h"
#include "history.h"
#include "jobs.h"
#include "terminal.h"
#include "io.h"
#include "globbing.h"
#include "alias.h"

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100

// Job control structures are defined in jobs.c

// Last command exit status (0 = success)
int last_status = 0;

// Function declarations
void print_prompt();
char *read_input();
char **parse_input(char *input, int *arg_count);
int execute_command(char **args, int arg_count, int background);
int execute_builtin(char **args);
void add_to_history(const char *command);
void show_history();
void check_background_jobs();
int parse_and_execute(char *input);
void execute_with_pipe(char *cmd1, char *cmd2);
/* redirection helpers in io.h */
void initialize_readline();

// New terminal control functions
/* init_shell_terminal moved to terminal.c */
void put_job_in_foreground(job_t *job, int cont);
void put_job_in_background(job_t *job, int cont);
void wait_for_job(job_t *job);
void mark_job_as_running(job_t *job);
void continue_job(job_t *job, int foreground);

// Helper: trim leading and trailing whitespace in place
static char *trim(char *s) {
  while (*s && (*s == ' ' || *s == '\t')) s++;
  char *end = s + strlen(s);
  while (end > s && (*(end - 1) == ' ' || *(end - 1) == '\t')) *(--end) = '\0';
  return s;
}

// after trim helper
static char *find_logic_op(char *s, int *is_and) {
  enum { NORM, SQ, DQ } st = NORM;
  for (char *p = s; *p; p++) {
    if (st == NORM) {
      if (*p == '\'')
        st = SQ;
      else if (*p == '"')
        st = DQ;
      else if (*p == '&' && *(p + 1) == '&') {
        *is_and = 1;
        return p;
      } else if (*p == '|' && *(p + 1) == '|') {
        *is_and = 0;
        return p;
      } else if (*p == '\\')
        p++;  // skip escaped char
    } else if (st == SQ) {
      if (*p == '\'') st = NORM;
    } else if (st == DQ) {
      if (*p == '"')
        st = NORM;
      else if (*p == '\\' && *(p + 1))
        p++;
    }
  }
  return NULL;
}

/**
 * Main function - where it all begins
 */
int main(int argc, char *argv[]) {
  char *input;

  // Set up our job system
  jobs_init();

  /* Handle -c option for one-liners */
  if (argc > 1 && strcmp(argv[1], "-c") == 0) {
    if (argc < 3) {
      fprintf(stderr, "ash: -c requires an argument\n");
      return 1;
    }

    /* Copy command and convert semicolons to newlines */
    size_t len_cmd = strlen(argv[2]);
    char *script = malloc(len_cmd + 2);
    for (size_t i = 0; i < len_cmd; i++) {
      script[i] = (argv[2][i] == ';') ? '\n' : argv[2][i];
    }
    script[len_cmd] = '\n';
    script[len_cmd + 1] = '\0';

    FILE *fp = fmemopen(script, len_cmd + 1, "r");
    if (!fp) {
      perror("fmemopen");
      free(script);
      return 1;
    }
    parse_stream(fp);
    fclose(fp);
    free(script);
    return 0;
  }

  /* Script execution mode */
  if (argc > 1) {
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
      perror("ash");
      return 1;
    }

    /* Set script arguments */
    for (int i = 2; i < argc; i++) {
      char num[16];
      snprintf(num, sizeof(num), "%d", i - 1);
      set_var(num, argv[i]);
    }

    parse_stream(fp);
    fclose(fp);
    return 0;
  }

  // Set up terminal and job control
  terminal_init();
  terminal_install_signal_handlers();
  initialize_readline();

  // Main loop - read, parse, execute
  while (1) {
    // Check for completed background jobs
    check_background_jobs();

    // Get user input
    input = read_input();
    if (input == NULL) {
      continue;
    }

    // Add non-empty commands to history
    if (strlen(input) > 0) {
      add_to_history(input);
    }

    // Do the thing
    parse_and_execute(input);
    free(input);
  }

  return 0;
}

/**
 * Display shell prompt
 * Note: This is no longer needed with readline but kept for compatibility
 */
void print_prompt() {
  // This function is now handled by readline in read_input()
}

/**
 * Read input from the user using readline
 */
char *read_input() {
  char prompt[MAX_INPUT_SIZE];
  char cwd[MAX_INPUT_SIZE];

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    // Make sure we don't overflow the prompt buffer
    char short_cwd[MAX_INPUT_SIZE / 2];
    if (strlen(cwd) > MAX_INPUT_SIZE / 2 - 10) {
      // If cwd is too long, use the last part only
      strcpy(short_cwd, "...");
      strncat(short_cwd, cwd + strlen(cwd) - (MAX_INPUT_SIZE / 2 - 13), MAX_INPUT_SIZE / 2 - 3);
    } else {
      strcpy(short_cwd, cwd);
    }
    snprintf(prompt, sizeof(prompt), "ash:%s> ", short_cwd);
  } else {
    strcpy(prompt, "ash> ");
  }

  char *input = readline(prompt);

  // Handle Ctrl+D (EOF)
  if (input == NULL) {
    printf("\nExiting shell...\n");
    exit(EXIT_SUCCESS);
  }

  // Add to readline history if not empty
  if (input[0] != '\0') {
    add_history(input);
  }

  return input;
}

/**
 * Parse input into command and arguments
 */
char **parse_input(char *input, int *arg_count) {
  char **args = malloc(MAX_ARGS * sizeof(char *));
  char *token;
  int position = 0;

  if (!args) {
    perror("malloc error");
    exit(EXIT_FAILURE);
  }

  token = strtok(input, " \t\r\n\a");
  while (token != NULL) {
    args[position] = token;
    position++;

    if (position >= MAX_ARGS) {
      fprintf(stderr, "Too many arguments\n");
      break;
    }

    token = strtok(NULL, " \t\r\n\a");
  }

  args[position] = NULL;
  *arg_count = position;
  return args;
}

/**
 * Handle built-in commands
 */
int execute_builtin(char **args) {
  if (args[0] == NULL) {
    return 1;
  }

  // Try the simple built-ins first
  if (handle_simple_builtin(args)) {
    return 1;
  }

  // history command
  if (strcmp(args[0], "history") == 0) {
    show_history();
    last_status = 0;
    return 1;
  }

  // jobs command
  if (strcmp(args[0], "jobs") == 0) {
    list_jobs();
    last_status = 0;
    return 1;
  }

  // fg command
  if (strcmp(args[0], "fg") == 0) {
    if (args[1] == NULL) {
      fprintf(stderr, "fg: job id required\n");
      last_status = 1;
      return 1;
    }

    int job_id = atoi(args[1]);
    for (int i = 0; i < MAX_JOBS; i++) {
      if (jobs[i].job_id == job_id) {
        printf("Bringing job %d to foreground: %s\n", job_id, jobs[i].command);
        continue_job(&jobs[i], 1);
        return 1;
      }
    }

    fprintf(stderr, "fg: no such job: %d\n", job_id);
    last_status = 1;
    return 1;
  }

  // bg command
  if (strcmp(args[0], "bg") == 0) {
    if (args[1] == NULL) {
      fprintf(stderr, "bg: job id required\n");
      last_status = 1;
      return 1;
    }

    int job_id = atoi(args[1]);
    for (int i = 0; i < MAX_JOBS; i++) {
      if (jobs[i].job_id == job_id) {
        printf("Running job %d in background: %s\n", job_id, jobs[i].command);
        continue_job(&jobs[i], 0);
        return 1;
      }
    }

    fprintf(stderr, "bg: no such job: %d\n", job_id);
    last_status = 1;
    return 1;
  }

  return 0;
}

/**
 * Execute external commands
 */
int execute_command(char **args, int arg_count, int background) {
  pid_t pid;
  pid_t pgid = 0;

  // Fork a child process
  pid = fork();

  if (pid == -1) {
    perror("fork error");
    return -1;
  } else if (pid == 0) {
    /* child */

    if (shell_is_interactive) {
      // Put us in our own process group
      pid = getpid();

      // First process becomes group leader
      if (pgid == 0) pgid = pid;

      setpgid(pid, pgid);

      // If running in foreground, take control of the terminal
      if (!background) tcsetpgrp(shell_terminal, pgid);

      // Reset signals to default behavior
      signal(SIGINT, SIG_DFL);   // Ctrl+C
      signal(SIGQUIT, SIG_DFL);  // Ctrl+backslash
      signal(SIGTSTP, SIG_DFL);  // Ctrl+Z
      signal(SIGTTIN, SIG_DFL);  // Background read
      signal(SIGTTOU, SIG_DFL);  // Background write
    }

    // Set up any I/O redirections
    handle_redirection(args, &arg_count);

    // Try to run the command
    if (execvp(args[0], args) == -1) {
      perror("exec error");
      exit(EXIT_FAILURE);
    }
  } else {
    /* parent */

    if (shell_is_interactive) {
      // Make sure child is in its process group
      if (pgid == 0) pgid = pid;

      setpgid(pid, pgid);
    } else {
      // Non-interactive mode: just wait for child
      int status;
      waitpid(pid, &status, 0);
      last_status = status ? 1 : 0;
      return 0;
    }

    // Build command string for job display
    char command[MAX_INPUT_SIZE] = "";
    for (int i = 0; i < arg_count; i++) {
      strcat(command, args[i]);
      if (i < arg_count - 1) strcat(command, " ");
    }

    // Add to our job list
    int job_id = add_job(pid, pgid, command, background);
    job_t *job = &jobs[job_id - 1];  // job_id is 1-based, array is 0-based

    if (background) {
      // Background job - print info and continue
      printf("[%d] %d\n", job_id, pid);
      put_job_in_background(job, 0);
      return 0;
    } else {
      // Foreground job - wait for it
      put_job_in_foreground(job, 0);

      // Was it stopped with Ctrl+Z?
      if (!job->running) {
        printf("\n[%d] Stopped: %s\n", job_id, job->command);
      } else {
        // Job finished, clean up
        last_status = 0;
        remove_job(job_id);
      }
    }
  }

  return 0;
}

/**
 * Move a job to the foreground
 */
void put_job_in_foreground(job_t *job, int cont) {
  if (job == NULL) return;

  // Let the job control the terminal
  tcsetpgrp(shell_terminal, job->pgid);

  // Send continue signal if needed
  if (cont && kill(-job->pgid, SIGCONT) < 0) perror("kill (SIGCONT)");

  // Update job status
  job->running = 1;
  job->foreground = 1;

  // Wait until it finishes or stops
  wait_for_job(job);

  // Take back terminal control
  tcsetpgrp(shell_terminal, shell_pgid);

  // Restore our terminal settings
  tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

/**
 * Move a job to the background
 */
void put_job_in_background(job_t *job, int cont) {
  if (job == NULL) return;

  // Send continue signal if needed
  if (cont && kill(-job->pgid, SIGCONT) < 0) perror("kill (SIGCONT)");

  // Just mark it as running in background
  job->running = 1;
  job->foreground = 0;
}

/**
 * Wait for a job to finish or stop
 */
void wait_for_job(job_t *job) {
  int status;
  pid_t pid;

  do {
    // Wait for any process in the job's process group
    pid = waitpid(-job->pgid, &status, WUNTRACED);

    if (pid < 0) {
      // Handle interruptions
      if (errno == EINTR) continue;

      perror("waitpid");
      return;
    }

    // Check what happened to the process
    if (WIFSTOPPED(status)) {
      // Process was stopped (Ctrl+Z)
      job->running = 0;
      return;
    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
      // Process finished or was killed
      if (pid == job->pid) {
        job->running = 0;
        return;
      }
    }
  } while (pid != job->pid);
}

/**
 * Mark job as running
 */
void mark_job_as_running(job_t *job) {
  if (job == NULL) return;

  job->running = 1;
  job->notified = 0;
}

/**
 * Continue a stopped job
 */
void continue_job(job_t *job, int foreground) {
  mark_job_as_running(job);

  if (foreground)
    put_job_in_foreground(job, 1);
  else
    put_job_in_background(job, 1);
}

/**
 * Handle command pipelines (cmd1 | cmd2)
 */
void execute_with_pipe(char *cmd1, char *cmd2) {
  int pipefd[2];
  pid_t pid1, pid2;
  pid_t pgid = 0;

  // Create the pipe
  if (pipe(pipefd) == -1) {
    perror("pipe error");
    return;
  }

  // First process (left side of pipe)
  pid1 = fork();
  if (pid1 < 0) {
    perror("fork error");
    return;
  }

  if (pid1 == 0) {
    // Child 1 - will write to pipe

    if (shell_is_interactive) {
      pid_t pid = getpid();

      // First process becomes group leader
      if (pgid == 0) pgid = pid;

      setpgid(pid, pgid);

      // Try to take terminal control
      if (tcsetpgrp(shell_terminal, pgid) < 0) perror("tcsetpgrp failed in first child");

      // Reset signals to defaults
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
    }

    // Close read end - we don't need it
    close(pipefd[0]);

    // Connect stdout to pipe
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
      perror("dup2 error");
      exit(EXIT_FAILURE);
    }
    close(pipefd[1]);

    // Parse and run the command
    int arg_count;
    char **args = split_command_line(cmd1, &arg_count);
    expand_aliases(&args, &arg_count);
    if (execute_builtin(args)) {
      free_tokens(args);
      exit(EXIT_SUCCESS);
    }
    // Try external command
    if (!execute_builtin(args)) {
      // Handle any redirections (except stdout which goes to pipe)
      handle_redirection(args, &arg_count);

      // Run the command
      if (execvp(args[0], args) == -1) {
        perror("exec error");
        exit(EXIT_FAILURE);
      }
    }

    free_tokens(args);
    exit(EXIT_SUCCESS);
  }

  // Remember process group for second child
  if (pgid == 0) pgid = pid1;

  // Make sure first child is in its process group
  if (shell_is_interactive) setpgid(pid1, pgid);

  // Second process (right side of pipe)
  pid2 = fork();
  if (pid2 < 0) {
    perror("fork error");
    return;
  }

  if (pid2 == 0) {
    // Child 2 - will read from pipe

    if (shell_is_interactive) {
      // Join the same process group
      setpgid(getpid(), pgid);

      // Reset signals
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
    }

    // Close write end - we don't need it
    close(pipefd[1]);

    // Connect stdin to pipe
    if (dup2(pipefd[0], STDIN_FILENO) == -1) {
      perror("dup2 error");
      exit(EXIT_FAILURE);
    }
    close(pipefd[0]);

    // Parse and run the command
    int arg_count;
    char **args = split_command_line(cmd2, &arg_count);
    expand_aliases(&args, &arg_count);
    if (execute_builtin(args)) {
      free_tokens(args);
      exit(EXIT_SUCCESS);
    }
    if (!execute_builtin(args)) {
      // Handle any redirections
      handle_redirection(args, &arg_count);

      // Run the command
      if (execvp(args[0], args) == -1) {
        perror("exec error");
        exit(EXIT_FAILURE);
      }
    }

    free_tokens(args);
    exit(EXIT_SUCCESS);
  }

  // Parent process
  if (!shell_is_interactive) {
    // Non-interactive mode: just wait for both children
    close(pipefd[0]);
    close(pipefd[1]);
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
    return;
  }

  // Make sure second child is in the pipeline's process group
  setpgid(pid2, pgid);

  // Close our copy of the pipe
  close(pipefd[0]);
  close(pipefd[1]);

  // Add job to our job list
  char pipeline_cmd[MAX_INPUT_SIZE];
  snprintf(pipeline_cmd, MAX_INPUT_SIZE, "%s | %s", cmd1, cmd2);
  int job_id = add_job(pid1, pgid, pipeline_cmd, 0);
  job_t *job = &jobs[job_id - 1];

  // Run in foreground
  put_job_in_foreground(job, 0);

  // Check if it was stopped
  if (!job->running) {
    printf("\n[%d] Stopped: %s\n", job_id, job->command);
  } else {
    // Job finished, clean up
    remove_job(job_id);
  }
}

/**
 * Set up readline with our preferences
 */
void initialize_readline() {
  // Tab key shows completion options
  rl_bind_key('\t', rl_complete);
}



// *** NEW: helper to split a command line into pipeline segments while respecting quotes ***
static int split_pipeline(char *input, char **segments, int max_segments) {
  enum { NORM, SQ, DQ } st = NORM;
  char *start = input;
  int count = 0;
  for (char *p = input; *p; p++) {
    if (st == NORM) {
      if (*p == '\\' && *(p + 1)) {
        p++;  // skip escaped char
        continue;
      }
      if (*p == '\'') {
        st = SQ;
      } else if (*p == '"') {
        st = DQ;
      } else if (*p == '|' && *(p + 1) != '|') {
        *p = '\0';
        if (count < max_segments) segments[count++] = trim(start);
        start = p + 1;
      }
    } else if (st == SQ) {
      if (*p == '\'') st = NORM;
    } else if (st == DQ) {
      if (*p == '"')
        st = NORM;
      else if (*p == '\\' && *(p + 1))
        p++;  // skip escaped char
    }
  }
  if (count < max_segments) segments[count++] = trim(start);
  return count;
}

// *** NEW: Execute an N-stage pipeline (segments[0..n-1]) ***
static void execute_pipeline(char **segments, int n, int background) {
  if (n <= 1) return;  // should not happen

  int pipefds[32][2];  // supports up to 33 cmds which is fine for now
  if (n - 1 > 32) {
    fprintf(stderr, "ash: too many pipeline stages\n");
    return;
  }

  // create required pipes beforehand
  for (int i = 0; i < n - 1; i++) {
    if (pipe(pipefds[i]) == -1) {
      perror("pipe");
      // close previously created pipes
      for (int k = 0; k < i; k++) {
        close(pipefds[k][0]);
        close(pipefds[k][1]);
      }
      return;
    }
  }

  pid_t pgid = 0;
  pid_t first_pid = 0;

  for (int i = 0; i < n; i++) {
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      // TODO: we could clean up children here but for simplicity just bail
      return;
    }

    if (pid == 0) {
      // Child process
      if (shell_is_interactive) {
        pid_t child_pid = getpid();
        if (pgid == 0)
          pgid = child_pid;  // the very first child sets pgid but parent hasn't updated yet
        // place ourselves into pgid (may already be set for later children)
        setpgid(child_pid, pgid);
        if (!background) {
          // Foreground: first child (or all) grabs terminal
          tcsetpgrp(shell_terminal, pgid);
        }
        // reset default signals
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
      }

      // Set up stdin/stdout depending on our position in pipeline
      if (i > 0) {
        // not first: connect stdin to previous pipe read end
        dup2(pipefds[i - 1][0], STDIN_FILENO);
      }
      if (i < n - 1) {
        // not last: connect stdout to current pipe write end
        dup2(pipefds[i][1], STDOUT_FILENO);
      }

      // Close all pipe fds in child (not needed anymore after dup)
      for (int k = 0; k < n - 1; k++) {
        close(pipefds[k][0]);
        close(pipefds[k][1]);
      }

      // Parse segment into argv
      int arg_count = 0;
      char **args = split_command_line(segments[i], &arg_count);
      expand_aliases(&args, &arg_count);

      // Built-in support inside pipeline (run in subshell)
      if (execute_builtin(args)) {
        free_tokens(args);
        _exit(EXIT_SUCCESS);
      }
      // Not a builtin -> external command
      if (!execute_builtin(args)) {
        handle_redirection(args, &arg_count);
        execvp(args[0], args);
        perror("exec");
      }
      free_tokens(args);
      _exit(EXIT_SUCCESS);
    }

    // Parent
    if (pgid == 0) {
      pgid = pid;  // first child sets the pgid for the pipeline
      first_pid = pid;
    }
    // ensure each child joins same pgid
    setpgid(pid, pgid);
  }

  // Parent: close all pipe fds
  for (int i = 0; i < n - 1; i++) {
    close(pipefds[i][0]);
    close(pipefds[i][1]);
  }

  // Now handle job control / waiting similar to execute_with_pipe()
  if (!shell_is_interactive) {
    // just wait synchronously for all children
    int status;
    for (int i = 0; i < n; i++) {
      waitpid(-pgid, &status, 0);
    }
    return;
  }

  // Build combined command string
  char pipeline_cmd[MAX_INPUT_SIZE] = "";
  for (int i = 0; i < n; i++) {
    strcat(pipeline_cmd, segments[i]);
    if (i < n - 1) strcat(pipeline_cmd, " | ");
  }

  int job_id = add_job(first_pid, pgid, pipeline_cmd, background);
  job_t *job = &jobs[job_id - 1];

  if (background) {
    printf("[%d] %d\n", job_id, first_pid);
    put_job_in_background(job, 0);
    return;
  }

  // Foreground: give terminal to pipeline and wait
  put_job_in_foreground(job, 0);
  if (!job->running) {
    printf("\n[%d] Stopped: %s\n", job_id, job->command);
  } else {
    remove_job(job_id);
  }
}

/**
 * Parse input and run the command
 */
int parse_and_execute(char *input) {
  // Nothing to do for empty input
  if (input == NULL || strlen(input) == 0) return 0;

  // Trim whitespace at both ends early
  input = trim(input);

  // Logical operators (&&, ||) handled first
  int is_and = 0;
  char *op_ptr = find_logic_op(input, &is_and);
  if (op_ptr) {
    char *right = op_ptr + 2;
    *op_ptr = '\0';
    char *left = trim(input);
    right = trim(right);
    int status_left = parse_and_execute(left);
    int status_total = status_left;
    if ((is_and && status_left == 0) || (!is_and && status_left != 0))
      status_total = parse_and_execute(right);
    last_status = status_total;
    return status_total;
  }

  // Background (&) detection (needs quoting awareness but keep simple)
  int background = 0;
  size_t len = strlen(input);
  if (len > 0) {
    // strip trailing spaces
    while (len > 0 && (input[len - 1] == ' ' || input[len - 1] == '\t')) {
      input[--len] = '\0';
    }
    if (len > 0 && input[len - 1] == '&') {
      background = 1;
      input[--len] = '\0';
      // remove any trailing spaces again
      while (len > 0 && (input[len - 1] == ' ' || input[len - 1] == '\t')) {
        input[--len] = '\0';
      }
    }
  }

  // Split pipelines
  char *segments[64];
  int seg_count = split_pipeline(input, segments, 64);
  if (seg_count > 1) {
    execute_pipeline(segments, seg_count, background);
    return 0;
  }

  // No pipeline – fall back to single command execution
  // Parse into argv
  int arg_count;
  char **args = split_command_line(segments[0], &arg_count);
  if (args[0] == NULL) {
    free_tokens(args);
    return 0;
  }
  expand_aliases(&args, &arg_count);
  if (execute_builtin(args)) {
    free_tokens(args);
    return 0;
  }
  // Variable assignment detection must come after alias expansion
  int all_assignments = 1;
  for (int i = 0; i < arg_count; i++) {
    char *eq = strchr(args[i], '=');
    if (!(eq && eq != args[i])) {
      all_assignments = 0;
      break;
    }
  }
  if (all_assignments) {
    for (int i = 0; i < arg_count; i++) {
      char *eq = strchr(args[i], '=');
      *eq = '\0';
      set_var(args[i], eq + 1);
    }
    free_tokens(args);
    return 0;
  }
  expand_vars(args, arg_count);
  expand_globs(&args, &arg_count);
  execute_command(args, arg_count, background);
  free_tokens(args);
  return 0;
}

