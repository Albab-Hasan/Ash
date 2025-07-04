/**
 * shell.c - A custom Unix-like shell implementation
 *
 * Features:
 * - Command execution
 * - Built-in commands (cd, exit, history)
 * - Background processes with &
 * - I/O redirection (>, >>, <)
 * - Pipeline support with |
 * - Job control and signal handling
 */

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
char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);

// New terminal control functions
/* init_shell_terminal moved to terminal.c */
void put_job_in_foreground(job_t *job, int cont);
void put_job_in_background(job_t *job, int cont);
void wait_for_job(job_t *job);
void mark_job_as_running(job_t *job);
void continue_job(job_t *job, int foreground);
pid_t create_process_group();

// Helper: trim leading and trailing whitespace in place
static char *trim(char *s)
{
  while (*s && (*s == ' ' || *s == '\t'))
    s++;
  char *end = s + strlen(s);
  while (end > s && (*(end - 1) == ' ' || *(end - 1) == '\t'))
    *(--end) = '\0';
  return s;
}

// after trim helper
static char *find_logic_op(char *s, int *is_and)
{
  enum
  {
    NORM,
    SQ,
    DQ
  } st = NORM;
  for (char *p = s; *p; p++)
  {
    if (st == NORM)
    {
      if (*p == '\'')
        st = SQ;
      else if (*p == '"')
        st = DQ;
      else if (*p == '&' && *(p + 1) == '&')
      {
        *is_and = 1;
        return p;
      }
      else if (*p == '|' && *(p + 1) == '|')
      {
        *is_and = 0;
        return p;
      }
      else if (*p == '\\')
        p++; // skip escaped char
    }
    else if (st == SQ)
    {
      if (*p == '\'')
        st = NORM;
    }
    else if (st == DQ)
    {
      if (*p == '"')
        st = NORM;
      else if (*p == '\\' && *(p + 1))
        p++;
    }
  }
  return NULL;
}

/**
 * Main function - shell entry point
 */
int main(int argc, char *argv[])
{
  char *input;

  // Initialize job subsystem
  jobs_init();

  /* Handle -c option to execute a single command and exit */
  if (argc > 1 && strcmp(argv[1], "-c") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "ash: -c requires an argument\n");
      return 1;
    }

    /* Copy command string and translate semicolons to newlines so that our script
       parser can understand multi-command one-liner including loops. */
    size_t len_cmd = strlen(argv[2]);
    char *script = malloc(len_cmd + 2); /* +1 for possible newline +1 for NUL */
    for (size_t i = 0; i < len_cmd; i++)
    {
      script[i] = (argv[2][i] == ';') ? '\n' : argv[2][i];
    }
    script[len_cmd] = '\n';
    script[len_cmd + 1] = '\0';

    FILE *fp = fmemopen(script, len_cmd + 1, "r");
    if (!fp)
    {
      perror("fmemopen");
      free(script);
      return 1;
    }
    parse_stream(fp);
    fclose(fp);
    free(script);
    return 0;
  }

  /* Non-interactive script execution: ash myscript.ash */
  if (argc > 1)
  {
    FILE *fp = fopen(argv[1], "r");
    if (!fp)
    {
      perror("ash");
      return 1;
    }

    /* Set positional parameters $1 $2 ... for the script */
    for (int i = 2; i < argc; i++)
    {
      char num[16];
      snprintf(num, sizeof(num), "%d", i - 1);
      set_var(num, argv[i]);
    }

    /* Use the parser to execute the whole script */
    parse_stream(fp);
    fclose(fp);
    return 0;
  }

  // Initialize terminal and set up job control
  terminal_init();

  // Set up signal handlers (Ctrl-C / Ctrl-Z for readline)
  terminal_install_signal_handlers();

  // Initialize readline
  initialize_readline();

  // Main shell loop
  while (1)
  {
    // Check if any background processes have completed
    check_background_jobs();

    // Read user input (no need to call print_prompt as readline handles it)
    input = read_input();
    if (input == NULL)
    {
      continue;
    }

    // Add command to history
    if (strlen(input) > 0)
    {
      add_to_history(input);
    }

    // Parse and execute the command
    parse_and_execute(input);

    // Free allocated memory
    free(input);
  }

  return 0;
}

/**
 * Display shell prompt
 * Note: This is no longer needed with readline but kept for compatibility
 */
void print_prompt()
{
  // This function is now handled by readline in read_input()
}

/**
 * Read input from the user using readline
 */
char *read_input()
{
  char prompt[MAX_INPUT_SIZE];
  char cwd[MAX_INPUT_SIZE];

  if (getcwd(cwd, sizeof(cwd)) != NULL)
  {
    // Make sure we don't overflow the prompt buffer
    char short_cwd[MAX_INPUT_SIZE / 2];
    if (strlen(cwd) > MAX_INPUT_SIZE / 2 - 10)
    {
      // If cwd is too long, use the last part only
      strcpy(short_cwd, "...");
      strncat(short_cwd, cwd + strlen(cwd) - (MAX_INPUT_SIZE / 2 - 13), MAX_INPUT_SIZE / 2 - 3);
    }
    else
    {
      strcpy(short_cwd, cwd);
    }
    snprintf(prompt, sizeof(prompt), "ash:%s> ", short_cwd);
  }
  else
  {
    strcpy(prompt, "ash> ");
  }

  char *input = readline(prompt);

  // Check for EOF
  if (input == NULL)
  {
    printf("\nExiting shell...\n");
    exit(EXIT_SUCCESS);
  }

  // Add command to readline history if it's not empty
  if (input[0] != '\0')
  {
    add_history(input);
  }

  return input;
}

/**
 * Parse input into command and arguments
 */
char **parse_input(char *input, int *arg_count)
{
  char **args = malloc(MAX_ARGS * sizeof(char *));
  char *token;
  int position = 0;

  if (!args)
  {
    perror("malloc error");
    exit(EXIT_FAILURE);
  }

  token = strtok(input, " \t\r\n\a");
  while (token != NULL)
  {
    args[position] = token;
    position++;

    if (position >= MAX_ARGS)
    {
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
int execute_builtin(char **args)
{
  if (args[0] == NULL)
  {
    return 1;
  }

  /* Delegate simple built-ins (cd, exit, source, export, let, etc.) to the
     new builtins.c implementation.  If it handled the command it returns 1
     and we can stop here. */
  if (handle_simple_builtin(args))
  {
    return 1;
  }

  // history command
  if (strcmp(args[0], "history") == 0)
  {
    show_history();
    last_status = 0;
    return 1;
  }

  // jobs command
  if (strcmp(args[0], "jobs") == 0)
  {
    list_jobs();
    last_status = 0;
    return 1;
  }

  // fg command
  if (strcmp(args[0], "fg") == 0)
  {
    if (args[1] == NULL)
    {
      fprintf(stderr, "fg: job id required\n");
      last_status = 1;
      return 1;
    }

    int job_id = atoi(args[1]);
    for (int i = 0; i < MAX_JOBS; i++)
    {
      if (jobs[i].job_id == job_id)
      {
        printf("Bringing job %d to foreground: %s\n", job_id, jobs[i].command);

        // Continue the job in the foreground
        continue_job(&jobs[i], 1);
        return 1;
      }
    }

    fprintf(stderr, "fg: no such job: %d\n", job_id);
    last_status = 1;
    return 1;
  }

  // bg command
  if (strcmp(args[0], "bg") == 0)
  {
    if (args[1] == NULL)
    {
      fprintf(stderr, "bg: job id required\n");
      last_status = 1;
      return 1;
    }

    int job_id = atoi(args[1]);
    for (int i = 0; i < MAX_JOBS; i++)
    {
      if (jobs[i].job_id == job_id)
      {
        printf("Running job %d in background: %s\n", job_id, jobs[i].command);

        // Continue the job in the background
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
int execute_command(char **args, int arg_count, int background)
{
  pid_t pid;
  pid_t pgid = 0;

  // Create child process
  pid = fork();

  if (pid == -1)
  {
    perror("fork error");
    return -1;
  }
  else if (pid == 0)
  {
    // Child process

    if (shell_is_interactive)
    {
      // Put the process into a new process group
      pid = getpid();

      // The process group ID is the same as the process ID of the first process
      if (pgid == 0)
      {
        pgid = pid;
      }

      setpgid(pid, pgid);

      // If foreground, give the process group control of the terminal
      if (!background)
      {
        tcsetpgrp(shell_terminal, pgid);
      }

      // Reset signal handlers to default for the child
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
    }

    // Handle redirection
    handle_redirection(args, &arg_count);

    // Execute the command
    if (execvp(args[0], args) == -1)
    {
      perror("exec error");
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    // Parent process

    if (shell_is_interactive)
    {
      // Put the child in its own process group
      if (pgid == 0)
      {
        pgid = pid;
      }

      setpgid(pid, pgid);
    }
    else
    {
      // Non-interactive: wait for child and return immediately
      int status;
      waitpid(pid, &status, 0);
      last_status = status ? 1 : 0;
      return 0;
    }

    // Rebuild command string for job control
    char command[MAX_INPUT_SIZE] = "";
    for (int i = 0; i < arg_count; i++)
    {
      strcat(command, args[i]);
      if (i < arg_count - 1)
      {
        strcat(command, " ");
      }
    }

    // Add job to job list
    int job_id = add_job(pid, pgid, command, background);
    job_t *job = &jobs[job_id - 1]; // job_id is 1-based, array is 0-based

    if (background)
    {
      // Background process
      printf("[%d] %d\n", job_id, pid);
      put_job_in_background(job, 0);
      return 0;
    }
    else
    {
      // Foreground process
      put_job_in_foreground(job, 0);

      // Check if process was stopped
      if (!job->running)
      {
        printf("\n[%d] Stopped: %s\n", job_id, job->command);
      }
      else
      {
        // Job completed, remove from job list
        last_status = 0;
        remove_job(job_id);
      }
    }
  }

  return 0;
}

/**
 * Put job in foreground
 */
void put_job_in_foreground(job_t *job, int cont)
{
  if (job == NULL)
  {
    return;
  }

  // Give terminal control to the job's process group
  tcsetpgrp(shell_terminal, job->pgid);

  // If continue is specified, send SIGCONT signal to the job
  if (cont)
  {
    if (kill(-job->pgid, SIGCONT) < 0)
    {
      perror("kill (SIGCONT)");
    }
  }

  // Mark job as running and in foreground
  job->running = 1;
  job->foreground = 1;

  // Wait for job to report
  wait_for_job(job);

  // Put the shell back in the foreground
  tcsetpgrp(shell_terminal, shell_pgid);

  // Restore the shell's terminal modes
  tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

/**
 * Put job in background
 */
void put_job_in_background(job_t *job, int cont)
{
  if (job == NULL)
  {
    return;
  }

  // If continue is specified, send SIGCONT signal to the job
  if (cont)
  {
    if (kill(-job->pgid, SIGCONT) < 0)
    {
      perror("kill (SIGCONT)");
    }
  }

  // Mark job as running and in background
  job->running = 1;
  job->foreground = 0;
}

/**
 * Wait for a job to complete
 */
void wait_for_job(job_t *job)
{
  int status;
  pid_t pid;

  do
  {
    pid = waitpid(-job->pgid, &status, WUNTRACED);
    if (pid < 0)
    {
      if (errno == EINTR)
      {
        // Interrupted by signal, just continue
        continue;
      }
      perror("waitpid");
      return;
    }

    // Check for job state changes
    if (WIFSTOPPED(status))
    {
      job->running = 0;
      return;
    }
    else if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      // If it's the job's process group leader, mark job as done
      if (pid == job->pid)
      {
        job->running = 0;
        return;
      }
    }
  } while (pid != job->pid);
}

/**
 * Mark job as running
 */
void mark_job_as_running(job_t *job)
{
  if (job == NULL)
  {
    return;
  }
  job->running = 1;
  job->notified = 0;
}

/**
 * Continue a stopped job
 */
void continue_job(job_t *job, int foreground)
{
  mark_job_as_running(job);

  if (foreground)
  {
    put_job_in_foreground(job, 1);
  }
  else
  {
    put_job_in_background(job, 1);
  }
}

/**
 * Execute a pipeline of commands
 */
void execute_with_pipe(char *cmd1, char *cmd2)
{
  int pipefd[2];
  pid_t pid1, pid2;
  pid_t pgid;

  if (pipe(pipefd) == -1)
  {
    perror("pipe error");
    return;
  }

  // Create a new process group for the pipeline
  pgid = 0;

  // First process
  pid1 = fork();
  if (pid1 < 0)
  {
    perror("fork error");
    return;
  }

  if (pid1 == 0)
  {
    // Child process 1

    if (shell_is_interactive)
    {
      pid_t pid = getpid();

      // First process in the pipeline becomes the process group leader
      if (pgid == 0)
      {
        pgid = pid;
      }

      setpgid(pid, pgid);

      // Give terminal control to the process group
      if (tcsetpgrp(shell_terminal, pgid) < 0)
      {
        perror("tcsetpgrp failed in first child");
      }

      // Reset signal handlers
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
    }

    close(pipefd[0]); // Close read end

    // Redirect stdout to pipe
    if (dup2(pipefd[1], STDOUT_FILENO) == -1)
    {
      perror("dup2 error");
      exit(EXIT_FAILURE);
    }
    close(pipefd[1]);

    // Parse and execute the command
    int arg_count;
    char **args = split_command_line(cmd1, &arg_count);

    // Check for built-in commands
    if (!execute_builtin(args))
    {
      // Handle any redirections
      handle_redirection(args, &arg_count);

      // Execute the command
      if (execvp(args[0], args) == -1)
      {
        perror("exec error");
        exit(EXIT_FAILURE);
      }
    }

    free_tokens(args);
    exit(EXIT_SUCCESS);
  }

  // Remember pgid for the second process
  if (pgid == 0)
  {
    pgid = pid1;
  }

  // Make sure first child is in its process group
  if (shell_is_interactive)
  {
    setpgid(pid1, pgid);
  }

  // Second process
  pid2 = fork();
  if (pid2 < 0)
  {
    perror("fork error");
    return;
  }

  if (pid2 == 0)
  {
    // Child process 2

    if (shell_is_interactive)
    {
      // Put in the same process group as the first process
      setpgid(getpid(), pgid);

      // Reset signal handlers
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
    }

    close(pipefd[1]); // Close write end

    // Redirect stdin from pipe
    if (dup2(pipefd[0], STDIN_FILENO) == -1)
    {
      perror("dup2 error");
      exit(EXIT_FAILURE);
    }
    close(pipefd[0]);

    // Parse and execute the command
    int arg_count;
    char **args = split_command_line(cmd2, &arg_count);

    // Check for built-in commands
    if (!execute_builtin(args))
    {
      // Handle any redirections
      handle_redirection(args, &arg_count);

      // Execute the command
      if (execvp(args[0], args) == -1)
      {
        perror("exec error");
        exit(EXIT_FAILURE);
      }
    }

    free_tokens(args);
    exit(EXIT_SUCCESS);
  }

  // Parent process
  if (!shell_is_interactive)
  {
    // Non-interactive: just wait for both children
    close(pipefd[0]);
    close(pipefd[1]);
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
    return;
  }

  // Interactive path – job control
  // Make sure second child is in the pipeline's process group
  setpgid(pid2, pgid);

  // Add job to job list
  char pipeline_cmd[MAX_INPUT_SIZE];
  snprintf(pipeline_cmd, MAX_INPUT_SIZE, "%s | %s", cmd1, cmd2);
  int job_id = add_job(pid1, pgid, pipeline_cmd, 0);
  job_t *job = &jobs[job_id - 1]; // job_id is 1-based, array is 0-based

  // Close pipe in parent
  close(pipefd[0]);
  close(pipefd[1]);

  // Put job in foreground
  put_job_in_foreground(job, 0);

  // Check if process was stopped
  if (!job->running)
  {
    printf("\n[%d] Stopped: %s\n", job_id, job->command);
  }
  else
  {
    // Job completed, remove from job list
    remove_job(job_id);
  }
}

/**
 * Initialize readline with our custom settings
 */
void initialize_readline()
{
  // Use our custom completion function
  rl_attempted_completion_function = command_completion;

  // Don't use the default filename completer
  rl_completion_entry_function = command_generator;

  // Display matches vertically
  rl_completion_display_matches_hook = rl_display_match_list;

  // Use a single tab to show completion options
  rl_bind_key('\t', rl_complete);
}

/**
 * Generate command completions
 */
char *command_generator(const char *text, int state)
{
  static DIR *dir;
  static struct dirent *entry;
  static char *executable_path;
  static int len;
  static int is_path;
  char *name;

  // If this is the first time called, initialize
  if (!state)
  {
    // Get the length of the text to match
    len = strlen(text);

    // Check if this is a path completion or command completion
    is_path = strchr(text, '/') != NULL;

    if (is_path)
    {
      // Extract the directory path
      char dir_path[MAX_INPUT_SIZE];
      strcpy(dir_path, text);

      // Find the last slash
      char *last_slash = strrchr(dir_path, '/');
      if (last_slash != NULL)
      {
        // If it's the first character, open the root directory
        if (last_slash == dir_path)
        {
          dir = opendir("/");
          executable_path = strdup("/");
        }
        else
        {
          *last_slash = '\0'; // Null-terminate at the last slash
          dir = opendir(dir_path);
          executable_path = strdup(dir_path);
        }
      }
      else
      {
        dir = opendir(".");
        executable_path = strdup(".");
      }
    }
    else
    {
      // Open the current directory
      dir = opendir(".");
      executable_path = strdup(".");

      // Also check directories in PATH for commands
      executable_path = strdup(text);
    }
  }

  // If we couldn't open the directory, return NULL
  if (!dir)
  {
    return NULL;
  }

  // Read entries from the directory
  while ((entry = readdir(dir)) != NULL)
  {
    name = entry->d_name;

    // Skip "." and ".."
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
      continue;
    }

    // Check if the name starts with the text
    if (strncmp(name, text, len) == 0)
    {
      char *result = malloc(strlen(name) + 1);
      if (!result)
      {
        perror("malloc error");
        return NULL;
      }

      strcpy(result, name);
      return result;
    }
  }

  // No more matches
  closedir(dir);
  dir = NULL;
  free(executable_path);

  return NULL;
}

/**
 * Custom completion function for readline
 */
char **command_completion(const char *text, int start, int end)
{
  // Prevent unused parameter warnings
  (void)start;
  (void)end;

  char **matches = NULL;

  // Generate completions
  matches = rl_completion_matches(text, command_generator);

  return matches;
}

/**
 * Parse input and handle pipes, redirections, and background execution
 */
int parse_and_execute(char *input)
{
  // Check for empty input
  if (input == NULL || strlen(input) == 0)
  {
    return 0;
  }

  // Handle && and || logical operators outside quotes
  int is_and = 0;
  char *op_ptr = find_logic_op(input, &is_and);
  if (op_ptr)
  {
    char *right = op_ptr + 2;
    *op_ptr = '\0';
    char *left = trim(input);
    right = trim(right);
    int status_left = parse_and_execute(left);
    int status_total = status_left;
    if ((is_and && status_left == 0) || (!is_and && status_left != 0))
    {
      status_total = parse_and_execute(right);
    }
    last_status = status_total;
    return status_total;
  }

  // Check for pipes
  char *pipe_token = strchr(input, '|');
  if (pipe_token != NULL)
  {
    *pipe_token = '\0'; // Split the string at pipe
    char *cmd1 = input;
    char *cmd2 = pipe_token + 1;

    // Remove leading spaces from second command
    while (*cmd2 == ' ' || *cmd2 == '\t')
    {
      cmd2++;
    }

    execute_with_pipe(cmd1, cmd2);
    return 0;
  }

  // Check for background execution
  int background = 0;
  int len = strlen(input);
  if (len > 0 && input[len - 1] == '&')
  {
    background = 1;
    input[len - 1] = '\0'; // Remove the & character

    // Remove trailing spaces
    len = strlen(input);
    while (len > 0 && (input[len - 1] == ' ' || input[len - 1] == '\t'))
    {
      input[len - 1] = '\0';
      len--;
    }
  }

  // Parse the input into command and arguments
  int arg_count;
  char **args = split_command_line(input, &arg_count);

  // Check for empty command
  if (args[0] == NULL)
  {
    free(args);
    return 0;
  }

  // Check for built-in commands
  if (execute_builtin(args))
  {
    free_tokens(args);
    return 0;
  }

  // Check for assignment(s)
  int all_assignments = 1;
  for (int i = 0; i < arg_count; i++)
  {
    char *eq = strchr(args[i], '=');
    if (eq == NULL || eq == args[i])
    {
      all_assignments = 0;
      break;
    }
  }
  if (all_assignments)
  {
    for (int i = 0; i < arg_count; i++)
    {
      char *eq = strchr(args[i], '=');
      *eq = '\0';
      const char *name = args[i];
      const char *value = eq + 1;
      set_var(name, value);
    }
    free(args);
    return 0;
  }

  // Expand variables in arguments
  expand_vars(args, arg_count);

  // Execute the command
  execute_command(args, arg_count, background);

  free_tokens(args);

  return 0;
}
