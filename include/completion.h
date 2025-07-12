#ifndef ASH_COMPLETION_H
#define ASH_COMPLETION_H

/* Enhanced completion system */

/* Built-in commands for completion */
extern const char *builtin_commands[];

/* Completion context */
typedef enum {
  COMPLETE_COMMAND,  /* First word - command name */
  COMPLETE_ARGUMENT, /* Subsequent words - arguments */
  COMPLETE_PATH,     /* Path completion */
  COMPLETE_VARIABLE  /* Variable completion */
} completion_context_t;

/* Get completion context based on current position */
completion_context_t get_completion_context(const char *line, int point);

/* Generate completions for different contexts */
char **complete_command(const char *text, int start, int end);
char **complete_argument(const char *text, int start, int end);
char **complete_path(const char *text, int start, int end);
char **complete_variable(const char *text, int start, int end);

/* Main completion entry point */
char **enhanced_completion(const char *text, int start, int end);

#endif /* ASH_COMPLETION_H */
