#ifndef LIBFLRL_FLRL_H
#define LIBFLRL_FLRL_H

#ifndef __has_builtin
# define __has_builtin(x) (0)
#endif

/* suppresses branch coverage (see lcovrc), and hints that the branch is
 * unlikely. usage:
 *
 * p = malloc(n);
 * if (MALLOC_FAILED(!p)) return -1;
 */
#if __has_builtin(__builtin_expect)
# define MALLOC_FAILED(x) __builtin_expect((x), 0)
#else
# define MALLOC_FAILED(x) (x)
#endif

#endif
