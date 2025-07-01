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

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_JOBS 32

// Job control structure
typedef struct job
{
  pid_t pid;
  int job_id;
  char command[MAX_INPUT_SIZE];
  int running;
} job_t;

// Global variables
char history[MAX_HISTORY][MAX_INPUT_SIZE];
int history_count = 0;
job_t jobs[MAX_JOBS];
int job_count = 0;

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
int add_job(pid_t pid, const char *command);
void remove_job(int job_id);
void list_jobs();
int parse_and_execute(char *input);
void execute_with_pipe(char *cmd1, char *cmd2);
void handle_redirection(char **args, int *arg_count);
void initialize_readline();
char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);

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
    jobs[i].job_id = -1;
    jobs[i].running = 0;
  }

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
      if (jobs[i].job_id == job_id && jobs[i].running)
      {
        pid_t pid = jobs[i].pid;
        printf("Bringing job %d to foreground: %s\n", job_id, jobs[i].command);

        // Send SIGCONT to continue the process
        kill(pid, SIGCONT);

        // Wait for it to finish
        waitpid(pid, NULL, WUNTRACED);

        // Remove the job
        remove_job(job_id);
        return 1;
      }
    }

    fprintf(stderr, "fg: no such job: %d\n", job_id);
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

    // Reset signal handlers to default for the child
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

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

    if (background)
    {
      // Background process
      int job_id = add_job(pid, command);
      printf("[%d] %d\n", job_id, pid);
      return 0;
    }
    else
    {
      // Foreground process
      int status;
      waitpid(pid, &status, WUNTRACED);

      // Check if process was stopped
      if (WIFSTOPPED(status))
      {
        printf("\n[%d] Stopped: %s\n", add_job(pid, command), command);
      }

      return status;
    }
  }

  return 0;
}

/**
 * Add job to job list
 */
int add_job(pid_t pid, const char *command)
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (jobs[i].pid == -1)
    {
      jobs[i].pid = pid;
      jobs[i].job_id = i + 1;
      jobs[i].running = 1;
      strcpy(jobs[i].command, command);
      job_count++;
      return jobs[i].job_id;
    }
  }

  fprintf(stderr, "Too many jobs\n");
  return -1;
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
      jobs[i].job_id = -1;
      jobs[i].running = 0;
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
    if (jobs[i].running)
    {
      printf("[%d] %d %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].command);
    }
  }
}

/**
 * Check for completed background jobs
 */
void check_background_jobs()
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
  {
    for (int i = 0; i < MAX_JOBS; i++)
    {
      if (jobs[i].pid == pid)
      {
        printf("\n[%d] Done: %s\n", jobs[i].job_id, jobs[i].command);
        remove_job(jobs[i].job_id);
        break;
      }
    }
  }
}

/**
 * Set up signal handlers
 */
void setup_signal_handlers()
{
  struct sigaction sa;

  // Set up SIGINT handler
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL) == -1)
  {
    perror("sigaction error");
    exit(EXIT_FAILURE);
  }

  // Set up SIGTSTP handler
  sa.sa_handler = handle_sigtstp;
  if (sigaction(SIGTSTP, &sa, NULL) == -1)
  {
    perror("sigaction error");
    exit(EXIT_FAILURE);
  }

  // Ignore SIGTTOU to prevent the shell from stopping when it tries
  // to access the terminal while in the background
  signal(SIGTTOU, SIG_IGN);
}

/**
 * SIGINT handler (Ctrl+C)
 */
void handle_sigint(int sig)
{
  (void)sig; // Prevent unused parameter warning
  printf("\n");
  print_prompt();
  fflush(stdout);
}

/**
 * SIGTSTP handler (Ctrl+Z)
 */
void handle_sigtstp(int sig)
{
  (void)sig; // Prevent unused parameter warning
  printf("\n");
  print_prompt();
  fflush(stdout);
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
 * Execute a pipeline of commands
 */
void execute_with_pipe(char *cmd1, char *cmd2)
{
  int pipefd[2];
  pid_t pid1, pid2;

  if (pipe(pipefd) == -1)
  {
    perror("pipe error");
    return;
  }

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
  close(pipefd[0]);
  close(pipefd[1]);

  // Wait for both children to complete
  waitpid(pid1, NULL, 0);
  waitpid(pid2, NULL, 0);
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

  // Execute the command
  pid_t pid = fork();

  if (pid == -1)
  {
    perror("fork error");
    free(args);
    return -1;
  }
  else if (pid == 0)
  {
    // Child process

    // Reset signal handlers to default for the child
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

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

    if (background)
    {
      // Background process
      int job_id = add_job(pid, command);
      printf("[%d] %d\n", job_id, pid);
    }
    else
    {
      // Foreground process
      int status;
      waitpid(pid, &status, WUNTRACED);

      // Check if process was stopped
      if (WIFSTOPPED(status))
      {
        printf("\n[%d] Stopped: %s\n", add_job(pid, command), command);
      }
    }
  }

  free(args);
  return 0;
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
