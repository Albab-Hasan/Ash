#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tokenizer.h"

int main(void)
{
  int argc = 0;
  char **argv = split_command_line("echo \"hello world\" end", &argc);
  assert(argc == 3);
  assert(strcmp(argv[1], "hello world") == 0);
  free_tokens(argv);

  argc = 0;
  argv = split_command_line("VAR='a b'", &argc);
  assert(argc == 1);
  assert(strcmp(argv[0], "VAR=a b") == 0);
  free_tokens(argv);

  printf("test_tokenizer_quotes: all tests passed\n");
  return 0;
}
