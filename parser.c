#include "parser.h"
#include "shell.h" // forward declaration to use parse_and_execute
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// For now we implement a trivial parser that reads line by line and executes directly.
// Proper recursive-descent parser will be filled in Phase-2 steps b-e.

static int eval_command(char *cmd)
{
  int rc = parse_and_execute(cmd);
  return rc == 0; // success -> true
}

static void exec_block(char **lines, int start, int end)
{
  char buffer[1024];
  for (int i = start; i < end; i++)
  {
    strcpy(buffer, lines[i]);
    parse_and_execute(buffer);
  }
}

// simplistic line reader storage
#define MAX_LINES 512

ASTNode *parse_stream(FILE *fp)
{
  char *lines[MAX_LINES];
  int n = 0;
  char line[1024];
  while (fgets(line, sizeof(line), fp) && n < MAX_LINES)
  {
    size_t len = strlen(line);
    if (len && line[len - 1] == '\n')
      line[len - 1] = '\0';
    lines[n++] = strdup(line);
  }

  int i = 0;
  while (i < n)
  {
    if (strncmp(lines[i], "if ", 3) == 0 || strcmp(lines[i], "if") == 0)
    {
      // parse condition until "then"
      char cond[1024] = "";
      char *p = strstr(lines[i], "then");
      int then_line = i;
      if (p)
      {
        // single-line condition with then on same line
        size_t len = p - lines[i] - 1;
        strncpy(cond, lines[i] + 3, len);
        cond[len] = '\0';
      }
      else
      {
        // condition spread across next lines until "then"
        strcpy(cond, lines[i] + 3);
        then_line++;
        while (then_line < n && strcmp(lines[then_line], "then") != 0)
        {
          strcat(cond, " ");
          strcat(cond, lines[then_line]);
          then_line++;
        }
      }
      // find else and fi
      int else_line = -1, fi_line = -1;
      int j = then_line + 1;
      int nested_if = 0;
      while (j < n)
      {
        if (strncmp(lines[j], "if", 2) == 0)
          nested_if++;
        if (strcmp(lines[j], "fi") == 0)
        {
          if (nested_if == 0)
          {
            fi_line = j;
            break;
          }
          else
            nested_if--;
        }
        else if (strcmp(lines[j], "else") == 0 && nested_if == 0)
        {
          else_line = j;
        }
        j++;
      }
      if (fi_line == -1)
      {
        fprintf(stderr, "parser: missing fi\n");
        break;
      }
      // Evaluate condition
      int cond_true = eval_command(cond);
      if (cond_true)
      {
        int body_start = then_line + 1;
        int body_end = (else_line == -1) ? fi_line : else_line;
        exec_block(lines, body_start, body_end);
      }
      else if (else_line != -1)
      {
        exec_block(lines, else_line + 1, fi_line);
      }
      i = fi_line + 1;
      continue;
    }
    /* --------------------------------------------------------------
     * while LOOP SUPPORT
     * Pattern handled (simplified):
     *   while <command ...> ; do            # "do" may be on same line or on following line
     *       # body lines
     *   done
     * We evaluate <command> with eval_command(). If exit status == 0, body executes and
     * condition is re-evaluated. Nested loops are supported via a simple counter.
     * -------------------------------------------------------------- */
    else if (strncmp(lines[i], "while ", 6) == 0 || strcmp(lines[i], "while") == 0)
    {
      /* 1. Locate the corresponding 'do' keyword that belongs to this while */
      char cond[1024] = "";
      int do_line = i;
      char *p_do = strstr(lines[i], "do");

      if (p_do && (p_do == lines[i] || *(p_do - 1) == ' '))
      {
        /*   "while <cond> do" or "while <cond>; do" all on one line */
        size_t len = p_do - (lines[i] + 6); /* skip "while " */
        strncpy(cond, lines[i] + 6, len);
        cond[len] = '\0';
      }
      else
      {
        /*   Condition continues onto next lines until we hit a standalone "do" */
        strcpy(cond, lines[i] + 6);
        do_line++;
        while (do_line < n && strcmp(lines[do_line], "do") != 0)
        {
          strcat(cond, " ");
          strcat(cond, lines[do_line]);
          do_line++;
        }
      }

      if (do_line >= n || strcmp(lines[do_line], "do") != 0)
      {
        fprintf(stderr, "parser: missing do in while-loop\n");
        break;
      }

      /* 2. Find matching 'done' for this loop (handle nesting) */
      int done_line = -1;
      int j = do_line + 1;
      int nested_while = 0;
      while (j < n)
      {
        if (strncmp(lines[j], "while", 5) == 0)
          nested_while++;
        if (strcmp(lines[j], "done") == 0)
        {
          if (nested_while == 0)
          {
            done_line = j;
            break;
          }
          else
            nested_while--;
        }
        j++;
      }

      if (done_line == -1)
      {
        fprintf(stderr, "parser: missing done in while-loop\n");
        break;
      }

      /* 3. Execute the loop */
      while (eval_command(cond))
      {
        exec_block(lines, do_line + 1, done_line);
      }

      /* 4. Move index past the loop */
      i = done_line + 1;
      continue;
    }
    else if (lines[i][0] == '\0')
    {
      i++;
      continue;
    }
    else
    {
      char buf[1024];
      strcpy(buf, lines[i]);
      parse_and_execute(buf);
      i++;
    }
  }

  for (int k = 0; k < n; k++)
    free(lines[k]);
  return NULL;
}

void free_ast(ASTNode *node)
{
  (void)node;
}

void exec_ast(ASTNode *node)
{
  (void)node;
}
