#ifndef ASH_ARITH_H
#define ASH_ARITH_H

#include <stddef.h>
long eval_arith(const char *expr, int *ok);
char *expand_arith_subst(const char *arg);

#endif
