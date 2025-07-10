#include "globbing.h"
#include "tokenizer.h" /* for free_tokens */

#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Helper: check if the word contains unescaped wildcard meta characters */
static int contains_glob_chars(const char *s) {
  /* Basic scan for typical wildcard metacharacters */
  for (const char *p = s; *p; p++) {
    if (*p == '*' || *p == '?' || *p == '[') return 1;
  }
  return 0;
}

void expand_globs(char ***args_ptr, int *arg_count) {
  if (!args_ptr || !*args_ptr || !arg_count) return;

  char **old = *args_ptr;
  int old_count = *arg_count;

  /* Initial capacity (grows as needed) */
  int cap = old_count + 16; /* start with some slack */
  char **newargv = malloc(cap * sizeof(char *));
  if (!newargv) {
    perror("malloc");
    return; /* leave original argv intact */
  }
  int newc = 0;

  for (int i = 0; i < old_count; i++) {
    char *arg = old[i];
    if (!arg) continue;

    /* If it doesn't look like a pattern, just copy. */
    if (!contains_glob_chars(arg)) {
      /* Ensure capacity */
      if (newc + 1 >= cap) {
        cap *= 2;
        char **tmp = realloc(newargv, cap * sizeof(char *));
        if (!tmp) {
          perror("realloc");
          goto bail; /* fall back to returning what we have */
        }
        newargv = tmp;
      }
      newargv[newc++] = strdup(arg);
      continue;
    }

    /* Use POSIX glob(3) to expand the pattern. */
    glob_t g;
    int flags = GLOB_ERR;
    int ret = glob(arg, flags, NULL, &g);
    if (ret == 0) {
      /* One or more matches. */
      for (size_t k = 0; k < g.gl_pathc; k++) {
        if (newc + 1 >= cap) {
          cap *= 2;
          char **tmp = realloc(newargv, cap * sizeof(char *));
          if (!tmp) {
            perror("realloc");
            globfree(&g);
            goto bail;
          }
          newargv = tmp;
        }
        newargv[newc++] = strdup(g.gl_pathv[k]);
      }
      globfree(&g);
    } else {
      /* No matches or error: keep the original literal. */
      if (ret == GLOB_NOMATCH) {
        if (newc + 1 >= cap) {
          cap *= 2;
          char **tmp = realloc(newargv, cap * sizeof(char *));
          if (!tmp) {
            perror("realloc");
            goto bail;
          }
          newargv = tmp;
        }
        newargv[newc++] = strdup(arg);
      } else {
        /* For errors other than no-match, print a warning and keep literal. */
        fprintf(stderr, "ash: globbing error for pattern '%s'\n", arg);
        if (newc + 1 >= cap) {
          cap *= 2;
          char **tmp = realloc(newargv, cap * sizeof(char *));
          if (!tmp) {
            perror("realloc");
            goto bail;
          }
          newargv = tmp;
        }
        newargv[newc++] = strdup(arg);
      }
    }
  }

  /* Sentinel */
  newargv[newc] = NULL;

  /* Swap in the expanded list */
  free_tokens(old);
  *args_ptr = newargv;
  *arg_count = newc;
  return;

bail:
  /* Clean up on allocation failure */
  for (int i = 0; i < newc; i++) free(newargv[i]);
  free(newargv);
  return;
}
