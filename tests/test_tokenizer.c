#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tokenizer.h"

int main(void)
{
  char line[] = "if var";
  int argc = 0;
  char **toks = tokenize_line(line, &argc);
  assert(argc == 2);
  assert(strcmp(toks[0], "if") == 0);
  assert(strcmp(toks[1], "var") == 0);
  free_tokens(toks);
  printf("test_tokenizer: all tests passed\n");
  return 0;
}
