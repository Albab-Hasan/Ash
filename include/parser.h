#ifndef ASH_PARSER_H
#define ASH_PARSER_H

#include <stdio.h>

typedef enum
{
  NODE_COMMAND,
  NODE_IF,
  NODE_WHILE,
  NODE_FOR,
} NodeType;

typedef struct ASTNode
{
  NodeType type;
  char **argv;                 // for simple commands
  struct ASTNode *cond;        // for control nodes: condition command
  struct ASTNode *body;        // first stmt in body (linked list via next)
  struct ASTNode *else_branch; // for if
  struct ASTNode *next;        // linked list of statements at same level
  char *var_name;              // for for-loop variable
  char **for_list;             // list of words in for loop
} ASTNode;

ASTNode *parse_stream(FILE *fp);
void free_ast(ASTNode *node);
void exec_ast(ASTNode *node);

/* Executes a user-defined shell function if it exists. Returns 1 if executed, 0 otherwise. */
int exec_function_if_defined(char **argv, int argc);

#endif
