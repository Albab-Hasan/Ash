#ifndef ASH_SYNTAX_H
#define ASH_SYNTAX_H

/* Syntax highlighting support */

/* Token types for highlighting */
typedef enum {
  TOKEN_COMMAND,  /* Built-in or executable */
  TOKEN_ARGUMENT, /* Regular argument */
  TOKEN_OPERATOR, /* |, >, <, >>, << */
  TOKEN_VARIABLE, /* $VAR */
  TOKEN_STRING,   /* Quoted strings */
  TOKEN_COMMENT,  /* # comments */
  TOKEN_NORMAL    /* Default */
} token_type_t;

/* Syntax highlighting entry */
typedef struct {
  int start;
  int end;
  token_type_t type;
} highlight_entry_t;

/* Highlight a line of shell code */
highlight_entry_t *highlight_line(const char *line, int *entry_count);

/* Free highlight entries */
void free_highlights(highlight_entry_t *entries);

/* Get color code for token type */
const char *get_token_color(token_type_t type);

#endif /* ASH_SYNTAX_H */
