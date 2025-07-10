#ifndef ASH_ALIAS_H
#define ASH_ALIAS_H

/* Simple alias handling */

void set_alias(const char *name, const char *value);
const char *get_alias(const char *name);
void unset_alias(const char *name);
void list_aliases(void);

/*
 * expand_aliases - replace the first word in *args_ptr if it matches an alias.
 * Safe to call repeatedly; performs up to 10 recursive expansions to avoid loops.
 *
 * On success, *args_ptr and *arg_count may be updated (old array freed via free_tokens).
 */
void expand_aliases(char ***args_ptr, int *arg_count);

#endif /* ASH_ALIAS_H */
