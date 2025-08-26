#ifndef LIBFLRL_XASSERT_H
#define LIBFLRL_XASSERT_H

#include "flrl/flrl.h"

#include <assert.h>
#include <stdbool.h>

/* hard_assert(expr):
 *
 * Assertion whose condition is always checked.  Normally, uses the standard
 * assertion mechanism.  When NDEBUG is defined (i.e. normal assertions will
 * be compiled out), hard_assert calls abort on failure instead.
 */
#if !defined(NDEBUG)
#define hard_assert(expr) assert(expr)
#else
#define hard_assert(expr) (void)((!!(expr)) || (abort(), 0))
#endif

/* bool soft_assert(expr):
 *
 *     if (!soft_assert(expr)) return -1;
 *
 * Assertion whose condition is always checked, and which evaluates to the
 * boolean result of the expression.  Normally, uses the standard assertion
 * mechanism to abort with a message if the condition is false.  When NDEBUG
 * is defined (i.e. normal assertions will be compiled out), the program
 * continues, but the result of soft_assert may be used to branch to custom
 * error handling.
 */
#define soft_assert(expr) (bool)(assert(expr),(!!(expr)))

#endif
