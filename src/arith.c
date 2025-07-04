#include "arith.h"
#include "vars.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Simple recursive-descent evaluator supporting + - * / % and parentheses */
static const char *p;
static int ok_flag;

static long parse_expr();
static long parse_term();
static long parse_factor();

static void skip_ws()
{
  while (*p && isspace((unsigned char)*p))
    p++;
}

static long parse_number()
{
  skip_ws();
  long val = 0;
  int neg = 0;
  if (*p == '-')
  {
    neg = 1;
    p++;
  }
  if (!isdigit((unsigned char)*p))
  {
    ok_flag = 0;
    return 0;
  }
  while (isdigit((unsigned char)*p))
  {
    val = val * 10 + (*p - '0');
    p++;
  }
  return neg ? -val : val;
}

static long parse_var()
{
  skip_ws();
  char name[64];
  int n = 0;
  while (isalnum((unsigned char)*p) || *p == '_')
  {
    if (n < 63)
      name[n++] = *p;
    p++;
  }
  name[n] = '\0';
  const char *v = get_var(name);
  if (!v)
  {
    ok_flag = 0;
    return 0;
  }
  return atol(v);
}

static long parse_factor()
{
  skip_ws();
  if (*p == '(')
  {
    p++;
    long v = parse_expr();
    skip_ws();
    if (*p != ')')
    {
      ok_flag = 0;
      return 0;
    }
    p++;
    return v;
  }
  if (isdigit((unsigned char)*p) || (*p == '-' && isdigit((unsigned char)*(p + 1))))
    return parse_number();
  else
    return parse_var();
}

static long parse_term()
{
  long v = parse_factor();
  while (1)
  {
    skip_ws();
    if (*p == '*')
    {
      p++;
      v *= parse_factor();
    }
    else if (*p == '/' || *p == '%')
    {
      int op = *p;
      p++;
      long rhs = parse_factor();
      if (rhs == 0)
      {
        ok_flag = 0;
        return 0;
      }
      v = (op == '/') ? v / rhs : v % rhs;
    }
    else
      break;
  }
  return v;
}

static long parse_expr()
{
  long v = parse_term();
  while (1)
  {
    skip_ws();
    if (*p == '+')
    {
      p++;
      v += parse_term();
    }
    else if (*p == '-')
    {
      p++;
      v -= parse_term();
    }
    else
      break;
  }
  return v;
}

long eval_arith(const char *expr, int *ok)
{
  p = expr;
  ok_flag = 1;
  long v = parse_expr();
  skip_ws();
  if (*p != '\0')
    ok_flag = 0;
  if (ok)
    *ok = ok_flag;
  return v;
}

/* Expand first $(( expr )) in arg, returns newly allocated string or NULL if none */
char *expand_arith_subst(const char *arg)
{
  const char *start = strstr(arg, "$((");
  if (!start)
    return NULL;
  const char *end = strstr(start + 3, "))");
  if (!end)
    return NULL;
  char expr[256];
  size_t elen = end - (start + 3);
  if (elen >= sizeof(expr))
    return NULL;
  memcpy(expr, start + 3, elen);
  expr[elen] = '\0';
  int ok;
  long val = eval_arith(expr, &ok);
  if (!ok)
    return NULL;
  char num[64];
  snprintf(num, sizeof(num), "%ld", val);
  size_t newlen = strlen(arg) - (end + 2 - start) + strlen(num) + 1;
  char *out = malloc(newlen);
  size_t prefix = start - arg;
  memcpy(out, arg, prefix);
  strcpy(out + prefix, num);
  strcat(out, end + 2);
  return out;
}
