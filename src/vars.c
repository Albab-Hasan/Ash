#include "vars.h"
#include "arith.h"
#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

/* Weak stub for unit tests (overridden by real implementation in shell.c) */
__attribute__((weak)) int parse_and_execute(char *input) {
  (void)input;
  return 0;
}

typedef struct {
  char name[MAX_VAR_NAME];
  char value[MAX_VAR_VALUE];
  int in_use;
} var_t;

static var_t vars[MAX_VARS];

static int find_var(const char *name) {
  for (int i = 0; i < MAX_VARS; i++) {
    if (vars[i].in_use && strcmp(vars[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

void set_var(const char *name, const char *value) {
  int idx = find_var(name);
  if (idx == -1) {
    // find empty slot
    for (int i = 0; i < MAX_VARS; i++) {
      if (!vars[i].in_use) {
        idx = i;
        vars[i].in_use = 1;
        strncpy(vars[i].name, name, MAX_VAR_NAME - 1);
        vars[i].name[MAX_VAR_NAME - 1] = '\0';
        break;
      }
    }
  }
  if (idx != -1) {
    strncpy(vars[idx].value, value, MAX_VAR_VALUE - 1);
    vars[idx].value[MAX_VAR_VALUE - 1] = '\0';
  } else {
    fprintf(stderr, "Variable table full\n");
  }
}

const char *get_var(const char *name) {
  int idx = find_var(name);
  if (idx != -1) {
    return vars[idx].value;
  }
  return NULL;
}

/**
 * Export a shell variable to the process environment so child processes inherit it.
 * If the variable is not defined, returns -1. Otherwise, calls setenv() and returns its result.
 */
int export_var(const char *name) {
  const char *val = get_var(name);
  if (!val) {
    return -1;
  }
  return setenv(name, val, 1); /* overwrite = 1 */
}

/**
 * Execute a command and capture its output
 * Returns a newly allocated string with the command output or NULL on failure
 */
char *capture_command_output(const char *cmd) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    return NULL;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    close(pipefd[0]);
    close(pipefd[1]);
    return NULL;
  }

  if (pid == 0) {
    /* child */
    close(pipefd[0]);  // Close read end

    // Redirect stdout to pipe
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }

    // Redirect stderr to /dev/null
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }

    // Close all other file descriptors
    close(pipefd[1]);

    // Execute the command
    parse_and_execute((char *)cmd);
    exit(EXIT_SUCCESS);
  } else {
    /* parent */
    close(pipefd[1]);  // Close write end

    // Read from pipe
    char buffer[4096];
    ssize_t bytes_read;
    size_t total_size = 0;
    size_t buffer_size = 4096;
    char *output = malloc(buffer_size);

    if (!output) {
      perror("malloc");
      close(pipefd[0]);
      return NULL;
    }

    output[0] = '\0';

    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';

      // Ensure output buffer is large enough
      if (total_size + bytes_read + 1 > buffer_size) {
        buffer_size *= 2;
        char *new_output = realloc(output, buffer_size);
        if (!new_output) {
          perror("realloc");
          free(output);
          close(pipefd[0]);
          return NULL;
        }
        output = new_output;
      }

      // Append to output
      strcat(output, buffer);
      total_size += bytes_read;
    }

    close(pipefd[0]);

    // Wait for child to finish
    int status;
    waitpid(pid, &status, 0);

    // Remove trailing newline if present
    if (total_size > 0 && output[total_size - 1] == '\n') {
      output[total_size - 1] = '\0';
    }

    return output;
  }
}

/**
 * Expand command substitution $(command) or `command` in the given string
 * Returns a newly allocated string with substitutions performed or NULL if no substitution
 */
char *expand_cmd_subst(const char *arg) {
  if (!arg || (strstr(arg, "$(") == NULL && strchr(arg, '`') == NULL)) return NULL;

  char *result = strdup(arg);
  if (!result) return NULL;

  // Handle $(command) syntax
  char *start;
  while ((start = strstr(result, "$(")) != NULL) {
    // Find the matching closing parenthesis
    int depth = 1;
    char *end = start + 2;
    while (*end && depth > 0) {
      if (*end == '(')
        depth++;
      else if (*end == ')')
        depth--;
      if (depth > 0) end++;
    }

    if (depth != 0 || !*end) {
      // Unmatched parenthesis
      fprintf(stderr, "Syntax error: unmatched $(\n");
      free(result);
      return NULL;
    }

    // Extract the command
    size_t cmd_len = end - (start + 2);
    char *cmd = malloc(cmd_len + 1);
    if (!cmd) {
      free(result);
      return NULL;
    }
    strncpy(cmd, start + 2, cmd_len);
    cmd[cmd_len] = '\0';

    // Execute the command and capture output
    char *cmd_output = capture_command_output(cmd);
    free(cmd);

    if (!cmd_output) {
      free(result);
      return NULL;
    }

    // Replace $(command) with its output
    size_t prefix_len = start - result;
    size_t suffix_len = strlen(end + 1);
    size_t output_len = strlen(cmd_output);
    size_t new_len = prefix_len + output_len + suffix_len + 1;

    char *new_result = malloc(new_len);
    if (!new_result) {
      free(cmd_output);
      free(result);
      return NULL;
    }

    // Copy parts together
    strncpy(new_result, result, prefix_len);
    new_result[prefix_len] = '\0';
    strcat(new_result, cmd_output);
    strcat(new_result, end + 1);

    free(cmd_output);
    free(result);
    result = new_result;
  }

  // Handle `command` syntax
  while ((start = strchr(result, '`')) != NULL) {
    char *end = strchr(start + 1, '`');
    if (!end) {
      // Unmatched backtick
      fprintf(stderr, "Syntax error: unmatched `\n");
      free(result);
      return NULL;
    }

    // Extract the command
    size_t cmd_len = end - (start + 1);
    char *cmd = malloc(cmd_len + 1);
    if (!cmd) {
      free(result);
      return NULL;
    }
    strncpy(cmd, start + 1, cmd_len);
    cmd[cmd_len] = '\0';

    // Execute the command and capture output
    char *cmd_output = capture_command_output(cmd);
    free(cmd);

    if (!cmd_output) {
      free(result);
      return NULL;
    }

    // Replace `command` with its output
    size_t prefix_len = start - result;
    size_t suffix_len = strlen(end + 1);
    size_t output_len = strlen(cmd_output);
    size_t new_len = prefix_len + output_len + suffix_len + 1;

    char *new_result = malloc(new_len);
    if (!new_result) {
      free(cmd_output);
      free(result);
      return NULL;
    }

    // Copy parts together
    strncpy(new_result, result, prefix_len);
    new_result[prefix_len] = '\0';
    strcat(new_result, cmd_output);
    strcat(new_result, end + 1);

    free(cmd_output);
    free(result);
    result = new_result;
  }

  return result;
}

void expand_vars(char **args, int arg_count) {
  for (int i = 0; i < arg_count; i++) {
    if (args[i] == NULL) continue;

    // Command substitution
    char *cmd_subst = expand_cmd_subst(args[i]);
    if (cmd_subst) {
      free(args[i]);
      args[i] = cmd_subst;
    }

    // Arithmetic expansion
    char *arith = expand_arith_subst(args[i]);
    if (arith) {
      free(args[i]);
      args[i] = arith;
    }

    // Variable expansion
    if (strchr(args[i], '$') && args[i][0] != '$') {
      // Handle embedded variables like prefix$VARsuffix
      char *result = strdup(args[i]);
      char *dollar;

      while ((dollar = strchr(result, '$')) != NULL && dollar[1] != '(') {
        // Find the end of the variable name
        char *end = dollar + 1;
        while (*end && (isalnum((unsigned char)*end) || *end == '_')) end++;

        // Extract the variable name
        size_t var_len = end - (dollar + 1);
        if (var_len == 0) continue;  // Just a $ with no name

        char var_name[MAX_VAR_NAME];
        if (var_len >= MAX_VAR_NAME) var_len = MAX_VAR_NAME - 1;

        strncpy(var_name, dollar + 1, var_len);
        var_name[var_len] = '\0';

        // Get the variable value
        const char *value = get_var(var_name);
        if (!value) value = "";  // Empty string for undefined variables

        // Replace $VAR with its value
        size_t prefix_len = dollar - result;
        size_t suffix_len = strlen(end);
        size_t value_len = strlen(value);
        size_t new_len = prefix_len + value_len + suffix_len + 1;

        char *new_result = malloc(new_len);
        if (!new_result) {
          free(result);
          return;
        }

        // Copy parts together
        strncpy(new_result, result, prefix_len);
        new_result[prefix_len] = '\0';
        strcat(new_result, value);
        strcat(new_result, end);

        free(result);
        result = new_result;
      }

      free(args[i]);
      args[i] = result;
    } else if (args[i][0] == '$' && args[i][1] != '(') {
      // Simple case: entire argument is a variable reference
      const char *val = get_var(args[i] + 1);
      if (val) {
        free(args[i]);
        args[i] = strdup(val);
      } else {
        free(args[i]);
        args[i] = strdup("");
      }
    }
  }
}
