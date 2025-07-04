#ifndef ASH_TOKENIZER_H
#define ASH_TOKENIZER_H

char **tokenize_line(char *line, int *argc);
void free_tokens(char **toks);
int is_keyword(const char *word);

/* Shell-aware splitter: handles quotes and escapes, returns NULL-terminated argv */
char **split_command_line(const char *line, int *argc);

#endif
