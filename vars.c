#include "vars.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  char name[MAX_VAR_NAME];
  char value[MAX_VAR_VALUE];
  int in_use;
} var_t;

static var_t vars[MAX_VARS];

static int find_var(const char *name)
{
  for (int i = 0; i < MAX_VARS; i++)
  {
    if (vars[i].in_use && strcmp(vars[i].name, name) == 0)
    {
      return i;
    }
  }
  return -1;
}

void set_var(const char *name, const char *value)
{
  int idx = find_var(name);
  if (idx == -1)
  {
    // find empty slot
    for (int i = 0; i < MAX_VARS; i++)
    {
      if (!vars[i].in_use)
      {
        idx = i;
        vars[i].in_use = 1;
        strncpy(vars[i].name, name, MAX_VAR_NAME - 1);
        vars[i].name[MAX_VAR_NAME - 1] = '\0';
        break;
      }
    }
  }
  if (idx != -1)
  {
    strncpy(vars[idx].value, value, MAX_VAR_VALUE - 1);
    vars[idx].value[MAX_VAR_VALUE - 1] = '\0';
  }
  else
  {
    fprintf(stderr, "Variable table full\n");
  }
}

const char *get_var(const char *name)
{
  int idx = find_var(name);
  if (idx != -1)
  {
    return vars[idx].value;
  }
  return NULL;
}

void expand_vars(char **args, int arg_count)
{
  for (int i = 0; i < arg_count; i++)
  {
    if (args[i] == NULL)
      continue;
    if (args[i][0] == '$')
    {
      const char *val = get_var(args[i] + 1);
      if (val)
      {
        args[i] = strdup(val);
      }
      else
      {
        // undefined => empty string
        args[i] = "";
      }
    }
  }
}
