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
#include "vars.h"

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_JOBS 32

// Job control structure
typedef struct job
{
  pid_t pid;                    // Process ID
  pid_t pgid;                   // Process group ID
  int job_id;                   // Job ID
  char command[MAX_INPUT_SIZE]; // Command string
  int running;                  // 1 if running, 0 if stopped
  int foreground;               // 1 if foreground, 0 if background
  int notified;                 // 1 if user was notified of status change
} job_t;

// Global variables
char history[MAX_HISTORY][MAX_INPUT_SIZE];
int history_count = 0;
job_t jobs[MAX_JOBS];
int job_count = 0;

// Terminal control globals
static pid_t shell_pgid;
static int shell_terminal;
static struct termios shell_tmodes;
static int shell_is_interactive;

// Function declarations
void print_prompt();
char *read_input();
char **parse_input(char *input, int *arg_count);
int execute_command(char **args, int arg_count, int background);
int execute_builtin(char **args);
void add_to_history(const char *command);
void show_history();
void setup_signal_handlers();
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void check_background_jobs();
int add_job(pid_t pid, pid_t pgid, const char *command, int bg);
void remove_job(int job_id);
void list_jobs();
int parse_and_execute(char *input);
void execute_with_pipe(char *cmd1, char *cmd2);
void handle_redirection(char **args, int *arg_count);
void initialize_readline();
char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);

// New terminal control functions
void init_shell_terminal();
void put_job_in_foreground(job_t *job, int cont);
void put_job_in_background(job_t *job, int cont);
job_t *find_job_by_pid(pid_t pid);
void wait_for_job(job_t *job);
void mark_job_as_running(job_t *job);
void continue_job(job_t *job, int foreground);
pid_t create_process_group();

/**
 * Main function - shell entry point
 */
int main()
{
  char *input;

  // Initialize job array
  for (int i = 0; i < MAX_JOBS; i++)
  {
    jobs[i].pid = -1;
    jobs[i].pgid = -1;
    jobs[i].job_id = -1;
    jobs[i].running = 0;
    jobs[i].foreground = 0;
    jobs[i].notified = 0;
  }

  // Initialize terminal and set up job control
  init_shell_terminal();

  // Set up signal handlers
  setup_signal_handlers();

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
 * Add command to history
 */
void add_to_history(const char *command)
{
  if (history_count == MAX_HISTORY)
  {
    // Shift history entries to make room
    for (int i = 1; i < MAX_HISTORY; i++)
    {
      strcpy(history[i - 1], history[i]);
    }
    history_count--;
  }

  strcpy(history[history_count], command);
  history_count++;
}

/**
 * Show command history
 */
void show_history()
{
  for (int i = 0; i < history_count; i++)
  {
    printf("%d: %s\n", i + 1, history[i]);
  }
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

  // cd command
  if (strcmp(args[0], "cd") == 0)
  {
    if (args[1] == NULL)
    {
      // Default to home directory
      if (chdir(getenv("HOME")) != 0)
      {
        perror("cd error");
      }
    }
    else
    {
      if (chdir(args[1]) != 0)
      {
        perror("cd error");
      }
    }
    return 1;
  }

  // exit command
  if (strcmp(args[0], "exit") == 0)
  {
    printf("Exiting shell...\n");
    exit(EXIT_SUCCESS);
  }

  // history command
  if (strcmp(args[0], "history") == 0)
  {
    show_history();
    return 1;
  }

  // jobs command
  if (strcmp(args[0], "jobs") == 0)
  {
    list_jobs();
    return 1;
  }

  // fg command
  if (strcmp(args[0], "fg") == 0)
  {
    if (args[1] == NULL)
    {
      fprintf(stderr, "fg: job id required\n");
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
    return 1;
  }

  // bg command
  if (strcmp(args[0], "bg") == 0)
  {
    if (args[1] == NULL)
    {
      fprintf(stderr, "bg: job id required\n");
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
    return 1;
  }

  // source command
  if (strcmp(args[0], "source") == 0)
  {
    if (args[1] == NULL)
    {
      fprintf(stderr, "source: filename required\n");
      return 1;
    }
    FILE *fp = fopen(args[1], "r");
    if (!fp)
    {
      perror("source");
      return 1;
    }
    char line[MAX_INPUT_SIZE];
    while (fgets(line, sizeof(line), fp))
    {
      // Remove trailing newline
      size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n')
      {
        line[len - 1] = '\0';
      }
      parse_and_execute(line);
    }
    fclose(fp);
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
 * Find job by pid
 */
job_t *find_job_by_pid(pid_t pid)
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].pid == pid)
    {
      return &jobs[i];
    }
  }
  return NULL;
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
 * Add job to job list
 */
int add_job(pid_t pid, pid_t pgid, const char *command, int bg)
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].pid == -1)
    {
      jobs[i].pid = pid;
      jobs[i].pgid = pgid;
      jobs[i].job_id = i + 1;
      jobs[i].running = 1;
      jobs[i].foreground = !bg;
      jobs[i].notified = 0;
      strcpy(jobs[i].command, command);
      job_count++;
      return jobs[i].job_id;
    }
  }

  fprintf(stderr, "Too many jobs\n");
  return -1;
}

/**
 * Check for completed background jobs
 */
void check_background_jobs()
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
  {
    job_t *job = find_job_by_pid(pid);

    if (job == NULL)
    {
      continue;
    }

    if (WIFSTOPPED(status))
    {
      job->running = 0;
      if (!job->notified)
      {
        printf("\n[%d] Stopped: %s\n", job->job_id, job->command);
        job->notified = 1;
      }
    }
    else if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      if (!job->notified)
      {
        printf("\n[%d] Done: %s\n", job->job_id, job->command);
        job->notified = 1;
        remove_job(job->job_id);
      }
    }
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
    char **args = parse_input(cmd1, &arg_count);

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

    free(args);
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
    char **args = parse_input(cmd2, &arg_count);

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

    free(args);
    exit(EXIT_SUCCESS);
  }

  // Parent process
  if (shell_is_interactive)
  {
    // Make sure second child is in the pipeline's process group
    setpgid(pid2, pgid);
  }

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
 * Set up signal handlers
 */
void setup_signal_handlers()
{
  // For job control in the shell, most signals are handled by the
  // signal() calls in init_shell_terminal()

  // Set up custom handlers for Ctrl-C and Ctrl-Z in the shell
  signal(SIGINT, handle_sigint);
  signal(SIGTSTP, handle_sigtstp);
}

/**
 * Handle I/O redirection
 */
void handle_redirection(char **args, int *arg_count)
{
  int i;
  int in_fd = -1, out_fd = -1;

  for (i = 0; i < *arg_count; i++)
  {
    if (args[i] == NULL)
    {
      break;
    }

    // Input redirection
    if (strcmp(args[i], "<") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: Missing filename after <\n");
        exit(EXIT_FAILURE);
      }

      in_fd = open(args[i + 1], O_RDONLY);
      if (in_fd == -1)
      {
        perror("open error");
        exit(EXIT_FAILURE);
      }

      // Redirect stdin
      if (dup2(in_fd, STDIN_FILENO) == -1)
      {
        perror("dup2 error");
        exit(EXIT_FAILURE);
      }

      // Remove redirection symbols and filename from arguments
      args[i] = NULL;
      *arg_count = i;
      break;
    }

    // Output redirection (overwrite)
    else if (strcmp(args[i], ">") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: Missing filename after >\n");
        exit(EXIT_FAILURE);
      }

      out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (out_fd == -1)
      {
        perror("open error");
        exit(EXIT_FAILURE);
      }

      // Redirect stdout
      if (dup2(out_fd, STDOUT_FILENO) == -1)
      {
        perror("dup2 error");
        exit(EXIT_FAILURE);
      }

      // Remove redirection symbols and filename from arguments
      args[i] = NULL;
      *arg_count = i;
      break;
    }

    // Output redirection (append)
    else if (strcmp(args[i], ">>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: Missing filename after >>\n");
        exit(EXIT_FAILURE);
      }

      out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (out_fd == -1)
      {
        perror("open error");
        exit(EXIT_FAILURE);
      }

      // Redirect stdout
      if (dup2(out_fd, STDOUT_FILENO) == -1)
      {
        perror("dup2 error");
        exit(EXIT_FAILURE);
      }

      // Remove redirection symbols and filename from arguments
      args[i] = NULL;
      *arg_count = i;
      break;
    }
  }

  // Close file descriptors after redirection
  if (in_fd != -1)
  {
    close(in_fd);
  }
  if (out_fd != -1)
  {
    close(out_fd);
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
 * Initialize the shell's terminal settings and job control
 */
void init_shell_terminal()
{
  // Check if we're running in a terminal
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive)
  {
    // Loop until we're in the foreground
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
    {
      kill(-shell_pgid, SIGTTIN);
    }

    // Ignore interactive and job-control signals
    signal(SIGINT, SIG_IGN);  // Ctrl-C
    signal(SIGQUIT, SIG_IGN); /* Ctrl-\ */
    signal(SIGTSTP, SIG_IGN); // Ctrl-Z
    signal(SIGTTIN, SIG_IGN); // Terminal input for bg process
    signal(SIGTTOU, SIG_IGN); // Terminal output for bg process

    // Put ourselves in our own process group
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0)
    {
      perror("Couldn't put the shell in its own process group");
      exit(EXIT_FAILURE);
    }

    // Grab control of the terminal
    tcsetpgrp(shell_terminal, shell_pgid);

    // Save default terminal attributes
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

/**
 * SIGINT handler (Ctrl+C)
 */
void handle_sigint(int sig)
{
  (void)sig; // Prevent unused parameter warning
  printf("\n");
  // Don't call print_prompt here as we use readline
  // just print a newline and let readline handle the rest
  rl_on_new_line();
  rl_redisplay();
}

/**
 * SIGTSTP handler (Ctrl+Z)
 */
void handle_sigtstp(int sig)
{
  (void)sig; // Prevent unused parameter warning
  printf("\n");
  // Don't call print_prompt here as we use readline
  // just print a newline and let readline handle the rest
  rl_on_new_line();
  rl_redisplay();
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
  char **args = parse_input(input, &arg_count);

  // Check for empty command
  if (args[0] == NULL)
  {
    free(args);
    return 0;
  }

  // Check for built-in commands
  if (execute_builtin(args))
  {
    free(args);
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

  free(args);
  return 0;
}

/**
 * Remove job from job list
 */
void remove_job(int job_id)
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].job_id == job_id)
    {
      jobs[i].pid = -1;
      jobs[i].pgid = -1;
      jobs[i].job_id = -1;
      jobs[i].running = 0;
      jobs[i].foreground = 0;
      jobs[i].notified = 0;
      jobs[i].command[0] = '\0';
      job_count--;
      break;
    }
  }
}

/**
 * List all jobs
 */
void list_jobs()
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].pid != -1)
    {
      const char *status = jobs[i].running ? "Running" : "Stopped";
      printf("[%d] %d %s\t%s\n", jobs[i].job_id, jobs[i].pid, status, jobs[i].command);
    }
  }
}
