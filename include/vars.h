#ifndef ASH_VARS_H
#define ASH_VARS_H

#define MAX_VARS 64
#define MAX_VAR_NAME 64
#define MAX_VAR_VALUE 256

void set_var(const char *name, const char *value);
const char *get_var(const char *name);
void expand_vars(char **args, int arg_count);

/* Export variable to process environment. Returns 0 on success, -1 if undefined or setenv failed */
int export_var(const char *name);

#endif // ASH_VARS_H
