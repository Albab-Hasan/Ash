#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "vars.h"

int parse_and_execute(char *line)
{
  if (strncmp(line, "print ", 6) == 0)
  {
    set_var("OUT", line + 6);
    return 0;
  }
  return 1;
}

int main(void)
{
  const char *script =
      "case apple in\n"
      "  banana) print banana ;;\n"
      "  a*) print match ;;\n"
      "esac\n";

  FILE *fp = fmemopen((void *)script, strlen(script), "r");
  parse_stream(fp);
  fclose(fp);
  const char *v = get_var("OUT");
  assert(v && strcmp(v, "match") == 0);
  printf("test_case: all tests passed\n");
  return 0;
}
