#include "history.h"
#include <string.h>
#include <stdio.h>

#define MAX_INPUT_SIZE 1024
#define MAX_HISTORY 100

static char history[MAX_HISTORY][MAX_INPUT_SIZE];
static int history_count = 0;

void add_to_history(const char *command)
{
  if (!command || command[0] == '\0')
    return;

  if (history_count == MAX_HISTORY)
  {
    /* Shift entries up to make room */
    for (int i = 1; i < MAX_HISTORY; i++)
    {
      strcpy(history[i - 1], history[i]);
    }
    history_count--;
  }

  strncpy(history[history_count], command, MAX_INPUT_SIZE - 1);
  history[history_count][MAX_INPUT_SIZE - 1] = '\0';
  history_count++;
}

void show_history(void)
{
  for (int i = 0; i < history_count; i++)
  {
    printf("%d: %s\n", i + 1, history[i]);
  }
}
