#include "builtins.h"
#include "vars.h"
#include "parser.h"
#include "arith.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int last_status;

int handle_simple_builtin(char **args) {
  if (args[0] == NULL) return 0;

  // cd
  if (strcmp(args[0], "cd") == 0) {
    const char *dir = args[1] ? args[1] : getenv("HOME");
    if (chdir(dir) != 0) {
      perror("cd");
      last_status = 1;
    } else
      last_status = 0;
    return 1;
  }

  // history handled in shell.c

  // exit
  if (strcmp(args[0], "exit") == 0) {
    printf("Exiting shell...\n");
    exit(EXIT_SUCCESS);
  }

  // source (delegates to parser)
  if (strcmp(args[0], "source") == 0) {
    if (!args[1]) {
      fprintf(stderr, "source: filename required\n");
      last_status = 1;
      return 1;
    }
    FILE *fp = fopen(args[1], "r");
    if (!fp) {
      perror("source");
      last_status = 1;
      return 1;
    }
    parse_stream(fp);
    fclose(fp);
    last_status = 0;
    return 1;
  }

  // export
  if (strcmp(args[0], "export") == 0) {
    if (!args[1]) {
      fprintf(stderr, "export: var required\n");
      last_status = 1;
      return 1;
    }
    for (int i = 1; args[i]; i++) {
      char *eq = strchr(args[i], '=');
      if (eq && eq != args[i]) {
        *eq = '\0';
        set_var(args[i], eq + 1);
        setenv(args[i], eq + 1, 1);
      } else if (export_var(args[i]) != 0) {
        fprintf(stderr, "export: %s undefined\n", args[i]);
        last_status = 1;
      }
    }
    last_status = 0;
    return 1;
  }

  // let builtin
  if (strcmp(args[0], "let") == 0) {
    int ok;
    long res = 0;
    for (int i = 1; args[i]; i++) {
      res = eval_arith(args[i], &ok);
    }
    last_status = (res == 0);
    return 1;
  }

  return 0;
}
