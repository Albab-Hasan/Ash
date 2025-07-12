#include "completion.h"
#include "builtins.h"
#include "vars.h"
#include <stdio.h>
#include <readline/readline.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

/* Built-in commands for completion */
const char *builtin_commands[] = {"cd",    "exit",    "history", "jobs",   "fg",  "bg",
                                  "alias", "unalias", "export",  "source", "let", NULL};

/* Get completion context based on current position */
completion_context_t get_completion_context(const char *line, int point) {
  if (point == 0) return COMPLETE_COMMAND;

  /* Count words before cursor */
  int word_count = 0;
  int in_word = 0;

  for (int i = 0; i < point; i++) {
    if (isspace(line[i])) {
      in_word = 0;
    } else if (!in_word) {
      in_word = 1;
      word_count++;
    }
  }

  if (word_count == 1) return COMPLETE_COMMAND;

  /* Check if we're completing a variable */
  if (line[point - 1] == '$') return COMPLETE_VARIABLE;

  /* Check if we're completing a path */
  for (int i = point - 1; i >= 0; i--) {
    if (isspace(line[i])) break;
    if (line[i] == '/') return COMPLETE_PATH;
  }

  return COMPLETE_ARGUMENT;
}

/* Complete built-in commands and executables */
char **complete_command(const char *text, int start, int end) {
  (void)start;
  (void)end;

  char **matches = NULL;
  int match_count = 0;
  int capacity = 10;

  matches = malloc(capacity * sizeof(char *));
  if (!matches) return NULL;

  /* Add built-in commands */
  for (int i = 0; builtin_commands[i]; i++) {
    if (strncmp(builtin_commands[i], text, strlen(text)) == 0) {
      if (match_count >= capacity) {
        capacity *= 2;
        char **tmp = realloc(matches, capacity * sizeof(char *));
        if (!tmp) goto cleanup;
        matches = tmp;
      }
      matches[match_count++] = strdup(builtin_commands[i]);
    }
  }

  /* Add executables from PATH */
  DIR *dir;
  struct dirent *entry;
  char *path = getenv("PATH");
  if (path) {
    char *path_copy = strdup(path);
    char *dir_path = strtok(path_copy, ":");

    while (dir_path) {
      dir = opendir(dir_path);
      if (dir) {
        while ((entry = readdir(dir)) != NULL) {
          if (strncmp(entry->d_name, text, strlen(text)) == 0) {
            /* Check if executable */
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            if (access(full_path, X_OK) == 0) {
              if (match_count >= capacity) {
                capacity *= 2;
                char **tmp = realloc(matches, capacity * sizeof(char *));
                if (!tmp) {
                  closedir(dir);
                  goto cleanup;
                }
                matches = tmp;
              }
              matches[match_count++] = strdup(entry->d_name);
            }
          }
        }
        closedir(dir);
      }
      dir_path = strtok(NULL, ":");
    }
    free(path_copy);
  }

  /* Add current directory executables */
  dir = opendir(".");
  if (dir) {
    while ((entry = readdir(dir)) != NULL) {
      if (strncmp(entry->d_name, text, strlen(text)) == 0) {
        if (access(entry->d_name, X_OK) == 0) {
          if (match_count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(matches, capacity * sizeof(char *));
            if (!tmp) {
              closedir(dir);
              goto cleanup;
            }
            matches = tmp;
          }
          matches[match_count++] = strdup(entry->d_name);
        }
      }
    }
    closedir(dir);
  }

  matches[match_count] = NULL;
  return matches;

cleanup:
  for (int i = 0; i < match_count; i++) free(matches[i]);
  free(matches);
  return NULL;
}

/* Complete arguments (files, directories) */
char **complete_argument(const char *text, int start, int end) {
  (void)start;
  (void)end;

  char **matches = NULL;
  int match_count = 0;
  int capacity = 10;

  matches = malloc(capacity * sizeof(char *));
  if (!matches) return NULL;

  DIR *dir;
  struct dirent *entry;

  /* Determine directory to search */
  char *last_slash = strrchr(text, '/');
  char *search_dir = ".";
  char *search_prefix = text;

  if (last_slash) {
    int dir_len = last_slash - text;
    search_dir = malloc(dir_len + 1);
    strncpy(search_dir, text, dir_len);
    search_dir[dir_len] = '\0';
    search_prefix = last_slash + 1;
  }

  dir = opendir(search_dir);
  if (dir) {
    while ((entry = readdir(dir)) != NULL) {
      if (strncmp(entry->d_name, search_prefix, strlen(search_prefix)) == 0) {
        if (match_count >= capacity) {
          capacity *= 2;
          char **tmp = realloc(matches, capacity * sizeof(char *));
          if (!tmp) {
            closedir(dir);
            goto cleanup;
          }
          matches = tmp;
        }

        /* Build full path for display */
        char *full_name;
        if (last_slash) {
          full_name = malloc(strlen(search_dir) + 1 + strlen(entry->d_name) + 1);
          sprintf(full_name, "%s/%s", search_dir, entry->d_name);
        } else {
          full_name = strdup(entry->d_name);
        }
        matches[match_count++] = full_name;
      }
    }
    closedir(dir);
  }

  if (last_slash) free(search_dir);

  matches[match_count] = NULL;
  return matches;

cleanup:
  for (int i = 0; i < match_count; i++) free(matches[i]);
  free(matches);
  return NULL;
}

/* Complete paths (same as argument completion) */
char **complete_path(const char *text, int start, int end) {
  return complete_argument(text, start, end);
}

/* Complete variables */
char **complete_variable(const char *text, int start, int end) {
  (void)start;
  (void)end;

  char **matches = NULL;
  int match_count = 0;
  int capacity = 10;

  matches = malloc(capacity * sizeof(char *));
  if (!matches) return NULL;

  /* Skip the $ prefix */
  const char *var_name = text + 1;

  /* Add common environment variables */
  const char *common_vars[] = {"HOME", "PATH", "USER", "SHELL", "PWD", NULL};
  for (int i = 0; common_vars[i]; i++) {
    if (strncmp(common_vars[i], var_name, strlen(var_name)) == 0) {
      if (match_count >= capacity) {
        capacity *= 2;
        char **tmp = realloc(matches, capacity * sizeof(char *));
        if (!tmp) goto cleanup;
        matches = tmp;
      }
      char *var_with_dollar = malloc(strlen(common_vars[i]) + 2);
      sprintf(var_with_dollar, "$%s", common_vars[i]);
      matches[match_count++] = var_with_dollar;
    }
  }

  matches[match_count] = NULL;
  return matches;

cleanup:
  for (int i = 0; i < match_count; i++) free(matches[i]);
  free(matches);
  return NULL;
}

/* Main completion entry point */
char **enhanced_completion(const char *text, int start, int end) {
  const char *line = rl_line_buffer;
  int point = rl_point;

  completion_context_t context = get_completion_context(line, point);

  switch (context) {
    case COMPLETE_COMMAND:
      return complete_command(text, start, end);
    case COMPLETE_ARGUMENT:
      return complete_argument(text, start, end);
    case COMPLETE_PATH:
      return complete_path(text, start, end);
    case COMPLETE_VARIABLE:
      return complete_variable(text, start, end);
    default:
      return complete_argument(text, start, end);
  }
}
