#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h" /* provides prototype for parse_and_execute */
#include "parser.h"
#include "vars.h"

/*
 * Minimal stub of parse_and_execute() for parser unit tests.
 * We only support:
 *   - Variable assignment lines  NAME=value
 *   - Command 'true'   (returns success / 0)
 * All other commands return failure / 1.
 * This is sufficient to test control-flow handling inside parser.c.
 */
int parse_and_execute(char *line)
{
  /* Trim leading spaces */
  while (*line == ' ' || *line == '\t')
    line++;

  if (strlen(line) == 0)
    return 0;

  char *eq = strchr(line, '=');
  if (eq && eq != line)
  {
    *eq = '\0';
    const char *name = line;
    char *val = eq + 1;
    if (val[0] == '$')
    {
      const char *rep = get_var(val + 1);
      if (rep)
        val = (char *)rep;
    }
    set_var(name, val);
    return 0; /* success */
  }

  if (strncmp(line, "true", 4) == 0)
    return 0;

  /* Default: non-zero status */
  return 1;
}

int main(void)
{
  /* Build a small script exercising if/while/for */
  const char *script =
      "X=0\n"
      "if true; then\n"
      "X=1\n"
      "else\n"
      "X=2\n"
      "fi\n"
      "for I in a b; do\n"
      "X=$I\n"
      "done\n";

  /* Use fmemopen to treat the string as a FILE* stream */
  FILE *fp = fmemopen((void *)script, strlen(script), "r");
  assert(fp && "fmemopen failed");

  parse_stream(fp);
  fclose(fp);

  /* After script executes, X should equal last item in for-list: "b" */
  const char *val = get_var("X");
  assert(val && strcmp(val, "b") == 0);

  printf("test_parser: all tests passed\n");
  return 0;
}
