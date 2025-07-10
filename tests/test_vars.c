#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vars.h"

int main(void) {
  /* basic set/get */
  set_var("FOO", "bar");
  const char *v = get_var("FOO");
  assert(v && strcmp(v, "bar") == 0);

  /* expand_vars on simple argv list */
  char *arg1 = strdup("$FOO");
  char *argv[] = {"echo", arg1, NULL};
  expand_vars(argv, 2);
  assert(strcmp(argv[1], "bar") == 0);

  printf("test_vars: all tests passed\n");
  return 0;
}
