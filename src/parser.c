#include "parser.h"
#include "shell.h"  // forward declaration to use parse_and_execute
#include "vars.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fnmatch.h>
#include <ctype.h>

// For now we implement a trivial parser that reads line by line and executes directly.
// Basic recursive-descent parser (can be extended further).

// Reuse trim helper locally (duplicates shell.c static function)
static char *trim(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  char *end = s + strlen(s);
  while (end > s && isspace((unsigned char)*(end - 1))) *(--end) = '\0';
  return s;
}

static int eval_command(char *cmd) {
  int rc = parse_and_execute(cmd);
  return rc == 0;  // success -> true
}

static int loop_control_flag = 0; /* 0=normal,1=break,2=continue */

static void reset_loop_flag() {
  loop_control_flag = 0;
}

static int is_break(const char *s) {
  return strcmp(s, "break") == 0;
}
static int is_continue(const char *s) {
  return strcmp(s, "continue") == 0;
}

static void exec_block(char **lines, int start, int end) {
  char buffer[1024];
  for (int i = start; i < end; i++) {
    char *line_trim = trim(lines[i]);
    if (is_break(line_trim)) {
      loop_control_flag = 1;
      break;
    }
    if (is_continue(line_trim)) {
      loop_control_flag = 2;
      break;
    }
    strcpy(buffer, line_trim);
    parse_and_execute(buffer);
    if (loop_control_flag) /* propagate flag */
      break;
  }
}

// simplistic line reader storage
#define MAX_LINES 512

// ---------------- Function support ------------------
#define MAX_FUNCS 32
typedef struct {
  char name[64];
  char **body;  // array of duplicated lines
  int line_count;
} func_t;

static func_t funcs[MAX_FUNCS];

/* Forward declarations (implementations appear below definitions) */
static int find_func(const char *name);
static int store_function(char *name, char **lines, int start, int end);
static int execute_function(const char *name, char **argv, int argc);

// after function table definitions and before parse_stream
static int find_func(const char *name) {
  for (int i = 0; i < MAX_FUNCS; i++) {
    if (funcs[i].line_count > 0 && strcmp(funcs[i].name, name) == 0) return i;
  }
  return -1;
}

static int store_function(char *name, char **lines, int start, int end) {
  int idx = find_func(name);
  if (idx == -1) {
    // find empty slot
    for (int i = 0; i < MAX_FUNCS; i++)
      if (funcs[i].line_count == 0) {
        idx = i;
        break;
      }
  }
  if (idx == -1) return -1;  // table full
  strncpy(funcs[idx].name, name, sizeof(funcs[idx].name) - 1);
  int cnt = end - start;
  funcs[idx].body = malloc(cnt * sizeof(char *));
  for (int k = 0; k < cnt; k++) {
    funcs[idx].body[k] = strdup(lines[start + k]);
  }
  funcs[idx].line_count = cnt;
  return 0;
}

static int execute_function(const char *name, char **argv, int argc) {
  int idx = find_func(name);
  if (idx == -1) return -1;
  // positional parameters
  for (int i = 1; i < argc; i++) {
    char num[16];
    snprintf(num, sizeof(num), "%d", i);
    set_var(num, argv[i]);
  }
  reset_loop_flag();
  exec_block(funcs[idx].body, 0, funcs[idx].line_count);
  return 0;
}

ASTNode *parse_stream(FILE *fp) {
  char *lines[MAX_LINES];
  int n = 0;
  char line[1024];
  while (fgets(line, sizeof(line), fp) && n < MAX_LINES) {
    size_t len = strlen(line);
    if (len && line[len - 1] == '\n') line[len - 1] = '\0';

    /* Split on semicolons to treat them as separate logical lines.
       Note: This is simplistic and does not honour quoting. */
    char *segment_ctx = NULL;
    char *seg = strtok_r(line, ";", &segment_ctx);
    while (seg && n < MAX_LINES) {
      /* Trim leading/trailing spaces */
      while (*seg == ' ' || *seg == '\t') seg++;
      size_t slen = strlen(seg);
      while (slen > 0 && (seg[slen - 1] == ' ' || seg[slen - 1] == '\t')) seg[--slen] = '\0';

      if (slen > 0) lines[n++] = strdup(seg);

      seg = strtok_r(NULL, ";", &segment_ctx);
    }
  }

  int i = 0;
  while (i < n) {
    if (strncmp(lines[i], "if ", 3) == 0 || strcmp(lines[i], "if") == 0) {
      // parse condition until "then"
      char cond[1024] = "";
      char *p = strstr(lines[i], "then");
      int then_line = i;
      if (p) {
        // single-line condition with then on same line
        size_t len = p - lines[i] - 1;
        strncpy(cond, lines[i] + 3, len);
        cond[len] = '\0';
      } else {
        // condition spread across next lines until "then"
        strcpy(cond, lines[i] + 3);
        then_line++;
        while (then_line < n && strcmp(lines[then_line], "then") != 0) {
          strcat(cond, " ");
          strcat(cond, lines[then_line]);
          then_line++;
        }
      }
      // find else and fi
      int else_line = -1, fi_line = -1;
      int j = then_line + 1;
      int nested_if = 0;
      while (j < n) {
        if (strncmp(lines[j], "if", 2) == 0) nested_if++;
        if (strcmp(lines[j], "fi") == 0) {
          if (nested_if == 0) {
            fi_line = j;
            break;
          } else
            nested_if--;
        } else if (strcmp(lines[j], "else") == 0 && nested_if == 0) {
          else_line = j;
        }
        j++;
      }
      if (fi_line == -1) {
        fprintf(stderr, "parser: missing fi\n");
        break;
      }
      // Evaluate condition
      int cond_true = eval_command(cond);
      if (cond_true) {
        int body_start = then_line + 1;
        int body_end = (else_line == -1) ? fi_line : else_line;
        exec_block(lines, body_start, body_end);
      } else if (else_line != -1) {
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
    else if (strncmp(lines[i], "while ", 6) == 0 || strcmp(lines[i], "while") == 0) {
      /* 1. Locate the corresponding 'do' keyword that belongs to this while */
      char cond[1024] = "";
      int do_line = i;
      char *p_do = strstr(lines[i], "do");

      if (p_do && (p_do == lines[i] || *(p_do - 1) == ' ')) {
        /*   "while <cond> do" or "while <cond>; do" all on one line */
        size_t len = p_do - (lines[i] + 6); /* skip "while " */
        strncpy(cond, lines[i] + 6, len);
        cond[len] = '\0';
      } else {
        /*   Condition continues onto next lines until we hit a standalone "do" */
        strcpy(cond, lines[i] + 6);
        do_line++;
        while (do_line < n && strcmp(lines[do_line], "do") != 0) {
          strcat(cond, " ");
          strcat(cond, lines[do_line]);
          do_line++;
        }
      }

      if (do_line >= n || strcmp(lines[do_line], "do") != 0) {
        fprintf(stderr, "parser: missing do in while-loop\n");
        break;
      }

      /* 2. Find matching 'done' for this loop (handle nesting) */
      int done_line = -1;
      int j = do_line + 1;
      int nested_while = 0;
      while (j < n) {
        if (strncmp(lines[j], "while", 5) == 0) nested_while++;
        if (strcmp(lines[j], "done") == 0) {
          if (nested_while == 0) {
            done_line = j;
            break;
          } else
            nested_while--;
        }
        j++;
      }

      if (done_line == -1) {
        fprintf(stderr, "parser: missing done in while-loop\n");
        break;
      }

      /* 3. Execute the loop */
      while (eval_command(cond)) {
        reset_loop_flag();
        exec_block(lines, do_line + 1, done_line);
        if (loop_control_flag == 1) /* break */
          break;
        if (loop_control_flag == 2) /* continue */
          continue;
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
    else if (strncmp(lines[i], "for ", 4) == 0 || strcmp(lines[i], "for") == 0) {
      char varname[64] = "";
      char itemlist[1024] = "";
      int do_line = -1;

      /* 1. Collect header lines until we hit "do" */
      int header_end = i;
      char header[1024] = "";
      strcpy(header, lines[i]);
      while (strstr(header, " do") == NULL && strcmp(lines[header_end], "do") != 0) {
        header_end++;
        if (header_end >= n) break;
        strcat(header, " ");
        strcat(header, lines[header_end]);
      }

      if (strstr(header, " do") == NULL &&
          (header_end >= n || strcmp(lines[header_end], "do") != 0)) {
        fprintf(stderr, "parser: malformed for-loop header (missing do)\n");
        break;
      }

      /* If header_end line contains only "do", set do_line; else we already have do in header */
      if (strcmp(lines[header_end], "do") == 0) {
        do_line = header_end;
      } else {
        do_line = header_end;  // 'do' token was on the same line we concatenated
      }

      /* 2. Tokenise header to extract var name and list */
      /* Expected tokens: for var in item1 item2 ... do */
      char *tok_ctx = NULL;
      char *tok = strtok_r(header, " \t", &tok_ctx);  // "for"
      if (!tok || strcmp(tok, "for") != 0) {
        fprintf(stderr, "parser: internal error parsing for-loop\n");
        break;
      }
      tok = strtok_r(NULL, " \t", &tok_ctx);  // var name
      if (!tok) {
        fprintf(stderr, "parser: missing variable name in for-loop\n");
        break;
      }
      strncpy(varname, tok, sizeof(varname) - 1);

      tok = strtok_r(NULL, " \t", &tok_ctx);  // expect "in"
      if (!tok || strcmp(tok, "in") != 0) {
        fprintf(stderr, "parser: missing 'in' keyword in for-loop\n");
        break;
      }

      /* The rest until "do" forms the item list */
      char *rest = strtok_r(NULL, "", &tok_ctx);  // get the rest of the string
      if (rest) {
        /* Strip trailing " do" if present */
        char *do_pos = strstr(rest, " do");
        if (do_pos) {
          *do_pos = '\0';
        }
        strncpy(itemlist, rest, sizeof(itemlist) - 1);
      }

      /* 3. Find matching 'done' (handle nesting) */
      int done_line = -1;
      int j = do_line + 1;
      int nested_for = 0;
      while (j < n) {
        if (strncmp(lines[j], "for", 3) == 0) nested_for++;
        if (strcmp(lines[j], "done") == 0) {
          if (nested_for == 0) {
            done_line = j;
            break;
          } else
            nested_for--;
        }
        j++;
      }

      if (done_line == -1) {
        fprintf(stderr, "parser: missing done in for-loop\n");
        break;
      }

      /* 4. Iterate over items */
      if (strlen(itemlist) == 0) {
        /* If no explicit list, default to positional parameters (not supported). Skip. */
        fprintf(stderr, "parser: empty item list in for-loop\n");
      } else {
        char *saveptr = NULL;
        char *item = strtok_r(itemlist, " \t", &saveptr);
        while (item) {
          /* Remove any trailing semicolon from item token */
          size_t ilen = strlen(item);
          if (ilen > 0 && item[ilen - 1] == ';') {
            item[ilen - 1] = '\0';
          }

          set_var(varname, item);
          reset_loop_flag();
          exec_block(lines, do_line + 1, done_line);
          if (loop_control_flag == 1) break; /* break loop */
          if (loop_control_flag == 2) {
            item = strtok_r(NULL, " \t", &saveptr);
            continue; /* continue next item */
          }

          /* Normal progression */
          item = strtok_r(NULL, " \t", &saveptr);
        }
      }

      /* 5. Advance index */
      i = done_line + 1;
      continue;
    }
    /* --------------------------------------------------------------
     * case STATEMENT SUPPORT (simple, non-nested)
     * Syntax (must be on separate lines):
     *   case WORD in
     *     PAT1) cmd1 ;;
     *     PAT2) cmd2 ;;
     *   esac
     * Only first matching pattern executes. PAT supports shell glob via fnmatch().
     * -------------------------------------------------------------- */
    else if (strncmp(lines[i], "case ", 5) == 0) {
      char word[256] = "";
      /* Extract WORD before 'in' */
      char *in_ptr = strstr(lines[i] + 5, " in");
      if (!in_ptr) {
        fprintf(stderr, "parser: malformed case header\n");
        break;
      }
      size_t wlen = in_ptr - (lines[i] + 5);
      strncpy(word, lines[i] + 5, wlen);
      word[wlen] = '\0';

      /* Locate matching esac */
      int esac_line = -1;
      int j = i + 1;
      while (j < n && strcmp(lines[j], "esac") != 0) j++;
      if (j >= n) {
        fprintf(stderr, "parser: missing esac\n");
        break;
      }

      /* Iterate over pattern lines between i+1 and j-1 */
      int executed = 0;
      for (int k = i + 1; k < j; k++) {
        if (executed) break;
        char *p = strchr(lines[k], ')');
        if (!p) continue;  // skip malformed line
        *p = '\0';
        char *pattern = trim(lines[k]);
        char *cmd = trim(p + 1);
        /* remove optional trailing ;; */
        size_t clen = strlen(cmd);
        if (clen >= 2 && cmd[clen - 1] == ';' && cmd[clen - 2] == ';') {
          cmd[clen - 2] = '\0';
        }

        if (fnmatch(pattern, word, 0) == 0) {
          executed = 1;
          parse_and_execute(cmd);
        }
      }

      i = esac_line = j + 1;
      continue;
    }
    /* Function definition: NAME() { */
    if (strstr(lines[i], "()") && strstr(lines[i], "{") && !strchr(lines[i], ' ')) {
      char fname[64];
      sscanf(lines[i], "%63[^() ]", fname);
      int body_start = i + 1;
      int brace_depth = 1;
      int j = i + 1;
      while (j < n && brace_depth > 0) {
        if (strchr(lines[j], '{')) brace_depth++;
        if (strchr(lines[j], '}')) brace_depth--;
        j++;
      }
      if (brace_depth != 0) {
        fprintf(stderr, "parser: missing } in function %s\n", fname);
        break;
      }
      store_function(fname, lines, body_start, j - 1);
      i = j;
      continue;
    } else if (lines[i][0] == '\0') {
      i++;
      continue;
    } else {
      char buf[1024];
      strcpy(buf, lines[i]);
      parse_and_execute(buf);
      i++;
    }
  }

  for (int k = 0; k < n; k++) free(lines[k]);
  return NULL;
}

void free_ast(ASTNode *node) {
  (void)node;
}

void exec_ast(ASTNode *node) {
  (void)node;
}

int exec_function_if_defined(char **argv, int argc) {
  if (argv == NULL || argv[0] == NULL) return 0;
  int idx = find_func(argv[0]);
  if (idx == -1) return 0;
  execute_function(argv[0], argv, argc);
  return 1;
}
