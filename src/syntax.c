#include "syntax.h"
#include "builtins.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Built-in commands for highlighting */
static const char *builtin_commands[] = {"cd",    "exit",    "history", "jobs",   "fg",  "bg",
                                         "alias", "unalias", "export",  "source", "let", NULL};

/* Operators for highlighting */
static const char *operators[] = {"|", ">", "<", ">>", "<<", "&&", "||", "&", NULL};

/* Check if string is in array */
static int is_in_array(const char *str, const char **array) {
  for (int i = 0; array[i]; i++) {
    if (strcmp(str, array[i]) == 0) return 1;
  }
  return 0;
}

/* Get next token from line */
static char *get_next_token(const char *line, int *pos, int *token_len) {
  int start = *pos;

  /* Skip whitespace */
  while (line[start] && isspace(line[start])) start++;

  if (!line[start]) {
    *pos = start;
    *token_len = 0;
    return NULL;
  }

  /* Handle quoted strings */
  if (line[start] == '"' || line[start] == '\'') {
    char quote = line[start];
    int end = start + 1;
    while (line[end] && line[end] != quote) {
      if (line[end] == '\\' && line[end + 1])
        end += 2;
      else
        end++;
    }
    if (line[end]) end++;
    *token_len = end - start;
    *pos = end;
    return strndup(line + start, *token_len);
  }

  /* Handle operators */
  for (int i = 0; operators[i]; i++) {
    int op_len = strlen(operators[i]);
    if (strncmp(line + start, operators[i], op_len) == 0) {
      *token_len = op_len;
      *pos = start + op_len;
      return strndup(line + start, op_len);
    }
  }

  /* Handle regular tokens */
  int end = start;
  while (line[end] && !isspace(line[end]) && line[end] != '|' && line[end] != '>' &&
         line[end] != '<' && line[end] != '&') {
    end++;
  }

  *token_len = end - start;
  *pos = end;
  return strndup(line + start, *token_len);
}

/* Determine token type */
static token_type_t get_token_type(const char *token, int position) {
  if (!token) return TOKEN_NORMAL;

  /* Comments */
  if (token[0] == '#') return TOKEN_COMMENT;

  /* Variables */
  if (token[0] == '$') return TOKEN_VARIABLE;

  /* Operators */
  if (is_in_array(token, operators)) return TOKEN_OPERATOR;

  /* Commands (first position or after operators) */
  if (position == 0 || (position > 0 && strchr("|&", token[0]))) {
    if (is_in_array(token, builtin_commands)) return TOKEN_COMMAND;
    /* Could check if executable here */
  }

  /* Quoted strings */
  if (token[0] == '"' || token[0] == '\'') return TOKEN_STRING;

  return TOKEN_ARGUMENT;
}

/* Highlight a line of shell code */
highlight_entry_t *highlight_line(const char *line, int *entry_count) {
  if (!line || !entry_count) return NULL;

  highlight_entry_t *entries = malloc(50 * sizeof(highlight_entry_t));
  if (!entries) return NULL;

  int entry_idx = 0;
  int pos = 0;
  int token_pos = 0;

  while (line[pos]) {
    int token_len;
    char *token = get_next_token(line, &pos, &token_len);

    if (!token || token_len == 0) {
      free(token);
      break;
    }

    token_type_t type = get_token_type(token, token_pos);

    /* Only add non-normal tokens */
    if (type != TOKEN_NORMAL) {
      entries[entry_idx].start = pos - token_len;
      entries[entry_idx].end = pos;
      entries[entry_idx].type = type;
      entry_idx++;
    }

    token_pos++;
    free(token);
  }

  *entry_count = entry_idx;
  return entries;
}

/* Free highlight entries */
void free_highlights(highlight_entry_t *entries) {
  free(entries);
}

/* Get color code for token type */
const char *get_token_color(token_type_t type) {
  switch (type) {
    case TOKEN_COMMAND:
      return "\033[1;32m"; /* Bold green */
    case TOKEN_OPERATOR:
      return "\033[1;33m"; /* Bold yellow */
    case TOKEN_VARIABLE:
      return "\033[1;36m"; /* Bold cyan */
    case TOKEN_STRING:
      return "\033[1;35m"; /* Bold magenta */
    case TOKEN_COMMENT:
      return "\033[1;30m"; /* Bold gray */
    case TOKEN_ARGUMENT:
      return "\033[0;37m"; /* White */
    default:
      return "\033[0m"; /* Reset */
  }
}
