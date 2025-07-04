#include "parser.h"
#include "shell.h" // forward declaration to use parse_and_execute
#include "vars.h"
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
    /* --------------------------------------------------------------
     * for LOOP SUPPORT
     * Pattern handled (simplified):
     *   for VAR in item1 item2 item3 ; do
     *       # body lines
     *   done
     *   - "in" may be on same line or on following line.
     *   - List items are whitespace-separated words (no glob expansion here; that is
     *     delegated to the underlying shell execution when the variable is expanded).
     * -------------------------------------------------------------- */
    else if (strncmp(lines[i], "for ", 4) == 0 || strcmp(lines[i], "for") == 0)
    {
      char varname[64] = "";
      char itemlist[1024] = "";
      int do_line = -1;

      /* 1. Collect header lines until we hit "do" */
      int header_end = i;
      char header[1024] = "";
      strcpy(header, lines[i]);
      while (strstr(header, " do") == NULL && strcmp(lines[header_end], "do") != 0)
      {
        header_end++;
        if (header_end >= n)
          break;
        strcat(header, " ");
        strcat(header, lines[header_end]);
      }

      if (strstr(header, " do") == NULL && (header_end >= n || strcmp(lines[header_end], "do") != 0))
      {
        fprintf(stderr, "parser: malformed for-loop header (missing do)\n");
        break;
      }

      /* If header_end line contains only "do", set do_line; else we already have do in header */
      if (strcmp(lines[header_end], "do") == 0)
      {
        do_line = header_end;
      }
      else
      {
        do_line = header_end; // 'do' token was on the same line we concatenated
      }

      /* 2. Tokenise header to extract var name and list */
      /* Expected tokens: for var in item1 item2 ... do */
      char *tok_ctx = NULL;
      char *tok = strtok_r(header, " \t", &tok_ctx); // "for"
      if (!tok || strcmp(tok, "for") != 0)
      {
        fprintf(stderr, "parser: internal error parsing for-loop\n");
        break;
      }
      tok = strtok_r(NULL, " \t", &tok_ctx); // var name
      if (!tok)
      {
        fprintf(stderr, "parser: missing variable name in for-loop\n");
        break;
      }
      strncpy(varname, tok, sizeof(varname) - 1);

      tok = strtok_r(NULL, " \t", &tok_ctx); // expect "in"
      if (!tok || strcmp(tok, "in") != 0)
      {
        fprintf(stderr, "parser: missing 'in' keyword in for-loop\n");
        break;
      }

      /* The rest until "do" forms the item list */
      char *rest = strtok_r(NULL, "", &tok_ctx); // get the rest of the string
      if (rest)
      {
        /* Strip trailing " do" if present */
        char *do_pos = strstr(rest, " do");
        if (do_pos)
        {
          *do_pos = '\0';
        }
        strncpy(itemlist, rest, sizeof(itemlist) - 1);
      }

      /* 3. Find matching 'done' (handle nesting) */
      int done_line = -1;
      int j = do_line + 1;
      int nested_for = 0;
      while (j < n)
      {
        if (strncmp(lines[j], "for", 3) == 0)
          nested_for++;
        if (strcmp(lines[j], "done") == 0)
        {
          if (nested_for == 0)
          {
            done_line = j;
            break;
          }
          else
            nested_for--;
        }
        j++;
      }

      if (done_line == -1)
      {
        fprintf(stderr, "parser: missing done in for-loop\n");
        break;
      }

      /* 4. Iterate over items */
      if (strlen(itemlist) == 0)
      {
        /* If no explicit list, default to positional parameters (not supported). Skip. */
        fprintf(stderr, "parser: empty item list in for-loop\n");
      }
      else
      {
        char *saveptr = NULL;
        char *item = strtok_r(itemlist, " \t", &saveptr);
        while (item)
        {
          /* Remove any trailing semicolon from item token */
          size_t ilen = strlen(item);
          if (ilen > 0 && item[ilen - 1] == ';')
          {
            item[ilen - 1] = '\0';
          }

          set_var(varname, item);
          exec_block(lines, do_line + 1, done_line);
          item = strtok_r(NULL, " \t", &saveptr);
        }
      }

      /* 5. Advance index */
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
