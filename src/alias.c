#include "alias.h"
#include "tokenizer.h"  // for free_tokens & split

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ALIASES 64

typedef struct {
  char name[64];
  char *value;  // heap-allocated
  int in_use;
} alias_t;

static alias_t aliases[MAX_ALIASES];

static int find_alias(const char *name) {
  for (int i = 0; i < MAX_ALIASES; i++) {
    if (aliases[i].in_use && strcmp(aliases[i].name, name) == 0) return i;
  }
  return -1;
}

void set_alias(const char *name, const char *value) {
  int idx = find_alias(name);
  if (idx == -1) {
    for (int i = 0; i < MAX_ALIASES; i++) {
      if (!aliases[i].in_use) {
        idx = i;
        break;
      }
    }
    if (idx == -1) {
      fprintf(stderr, "alias: table full\n");
      return;
    }
    aliases[idx].in_use = 1;
    strncpy(aliases[idx].name, name, sizeof(aliases[idx].name) - 1);
    aliases[idx].name[sizeof(aliases[idx].name) - 1] = '\0';
  }
  free(aliases[idx].value);
  aliases[idx].value = strdup(value);
}

const char *get_alias(const char *name) {
  int idx = find_alias(name);
  return (idx == -1) ? NULL : aliases[idx].value;
}

void unset_alias(const char *name) {
  int idx = find_alias(name);
  if (idx != -1) {
    free(aliases[idx].value);
    aliases[idx].value = NULL;
    aliases[idx].in_use = 0;
  }
}

void list_aliases(void) {
  for (int i = 0; i < MAX_ALIASES; i++) {
    if (aliases[i].in_use) {
      printf("alias %s='%s'\n", aliases[i].name, aliases[i].value);
    }
  }
}

void expand_aliases(char ***args_ptr, int *arg_count) {
  if (!args_ptr || !*args_ptr || !arg_count || *arg_count == 0) return;

  int depth = 0;
  char **args = *args_ptr;

  while (depth++ < 10) {
    const char *avalue = get_alias(args[0]);
    if (!avalue) return;  // no alias, done

    // Split alias value into tokens
    int valc = 0;
    char **valv = split_command_line(avalue, &valc);
    if (valc == 0) {  // alias to nothing -> treat as done
      free_tokens(valv);
      return;
    }

    // Build new argv = valv + (args + 1)
    int newc = valc + (*arg_count) - 1;
    char **newv = malloc((newc + 1) * sizeof(char *));
    int pos = 0;
    for (int i = 0; i < valc; i++) newv[pos++] = strdup(valv[i]);
    for (int i = 1; i < *arg_count; i++) newv[pos++] = strdup(args[i]);
    newv[newc] = NULL;

    free_tokens(valv);
    free_tokens(args);

    *args_ptr = args = newv;
    *arg_count = newc;
  }
  // Too deep possible alias loop.
}
