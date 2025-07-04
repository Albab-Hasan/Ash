#include "tokenizer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 256

static int is_special_char(char c) { return (c == ';' || c == '\n' || c == '(' || c == ')'); }
static int is_word_delim(char c) { return isspace((unsigned char)c) || is_special_char(c); }

int is_keyword(const char *w)
{
  const char *kw[] = {"if", "then", "fi", "while", "do", "done", "for", "in", NULL};
  for (int i = 0; kw[i]; i++)
  {
    if (strcmp(kw[i], w) == 0)
      return 1;
  }
  return 0;
}

char **tokenize_line(char *line, int *argc)
{
  char **toks = malloc(MAX_TOKENS * sizeof(char *));
  int count = 0;
  char *p = line;
  while (*p)
  {
    while (isspace((unsigned char)*p))
      p++;
    if (!*p)
      break;
    if (is_special_char(*p))
    {
      char tmp[2] = {*p, 0};
      toks[count++] = strdup(tmp);
      p++;
      continue;
    }
    char *start = p;
    while (*p && !is_word_delim(*p))
      p++;
    size_t len = p - start;
    char *word = strndup(start, len);
    toks[count++] = word;
  }
  toks[count] = NULL;
  *argc = count;
  return toks;
}

void free_tokens(char **toks)
{
  if (!toks)
    return;
  for (int i = 0; toks[i]; i++)
    free(toks[i]);
  free(toks);
}

/* -------------------------------------------------------------
 * Advanced tokenizer for command arguments.
 * Supports:
 *   - Whitespace separation
 *   - Single quotes preserve literal contents
 *   - Double quotes allow escape sequences (\\, \"), otherwise literal
 *   - Backslash escapes next char outside quotes
 * Returns NULL-terminated array similar to argv; caller must free with free_tokens().
 */
char **split_command_line(const char *line, int *argc)
{
  char **args = malloc(MAX_TOKENS * sizeof(char *));
  int count = 0;

  const char *p = line;
  char token[1024];
  int tpos = 0;

  enum
  {
    NORMAL,
    IN_SQUOTE,
    IN_DQUOTE
  } state = NORMAL;

  while (*p)
  {
    char c = *p;

    if (state == NORMAL)
    {
      if (c == ' ' || c == '\t')
      {
        if (tpos > 0)
        {
          token[tpos] = '\0';
          args[count++] = strdup(token);
          tpos = 0;
        }
        p++;
        continue;
      }
      else if (c == '\'')
      {
        state = IN_SQUOTE;
      }
      else if (c == '"')
      {
        state = IN_DQUOTE;
      }
      else if (c == '\\')
      {
        p++;
        if (*p)
          token[tpos++] = *p;
      }
      else
      {
        token[tpos++] = c;
      }
    }
    else if (state == IN_SQUOTE)
    {
      if (c == '\'')
      {
        state = NORMAL;
      }
      else
      {
        token[tpos++] = c;
      }
    }
    else if (state == IN_DQUOTE)
    {
      if (c == '"')
      {
        state = NORMAL;
      }
      else if (c == '\\' && (*(p + 1) == '"' || *(p + 1) == '\\'))
      {
        p++;
        token[tpos++] = *p;
      }
      else
      {
        token[tpos++] = c;
      }
    }
    p++;
  }

  if (tpos > 0)
  {
    token[tpos] = '\0';
    args[count++] = strdup(token);
  }

  args[count] = NULL;
  if (argc)
    *argc = count;
  return args;
}
