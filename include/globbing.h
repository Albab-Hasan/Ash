#ifndef ASH_GLOBBING_H
#define ASH_GLOBBING_H

/*
 * expand_globs - perform wildcard expansion on the given argv array.
 *
 * On input:
 *   - *args_ptr points to a NULL-terminated argument array (like argv)
 *   - *arg_count is the number of arguments (excluding the NULL terminator)
 *
 * After the call:
 *   - The original array is freed (using free_tokens)
 *   - *args_ptr is updated to point to a _new_ array that includes any
 *     pathname matches for wildcard patterns (*, ?, [abc] etc.)
 *   - *arg_count is updated to the new argument count.
 */
void expand_globs(char ***args_ptr, int *arg_count);

#endif /* ASH_GLOBBING_H */
