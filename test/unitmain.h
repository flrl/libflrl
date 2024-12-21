#ifndef LIBFLRL_TEST_UNITMAIN_H
#define LIBFLRL_TEST_UNITMAIN_H

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* cmocka.h depends on the above already being included */
#include <cmocka.h>

#define NO_STATE void **state __attribute__((unused))

extern int verbose;

extern const char *const um_group_name;
extern const struct CMUnitTest um_group_tests[];
extern const size_t um_group_n_tests;
extern CMFixtureFunction um_group_setup;
extern CMFixtureFunction um_group_teardown;

#undef assert_float_equal
#define assert_float_equal(a, b, e) do {                    \
    (void) (e);                                             \
    my_assert_float_equal((a), (b), __FILE__, __LINE__);    \
} while (0)
extern void my_assert_float_equal(float a, float b,
                                  const char *const file, const int line);

#define assert_int_in_range(v, l, u)                        \
    my_assert_int_in_range((v), (l), (u), __FILE__, __LINE__)
extern void my_assert_int_in_range(const intmax_t value,
                                   const intmax_t range_min,
                                   const intmax_t range_max,
                                   const char *const file,
                                   const int line);

#define assert_string_prefix_equal(a, b, l)                                 \
    my_assert_string_prefix_equal((a), (b), (l), __FILE__, __LINE__)
extern void my_assert_string_prefix_equal(const char *a,
                                          const char *b,
                                          int len,
                                          const char *const file,
                                          const int line);

#endif
