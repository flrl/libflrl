#include "unitmain.h"

#include "flrl/fputil.h"

#include <assert.h>
#include <float.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

int verbose;

static int list_group_tests(const char *name,
                            const struct CMUnitTest tests[],
                            const size_t n_tests);

static int usage(const char *argv0)
{
    fputs("usage:\n", stderr);
    fprintf(stderr, "    %s [-v] [-S] [filter]\n", argv0);
    fprintf(stderr, "    %s [-l]\n", argv0);
    fflush(stderr);
    return 64; // EX_USAGE
}

int main(int argc, char **argv)
{
    const char *filter = NULL;
    const char *outfmt = NULL;
    bool skip_filter = false;
    bool do_list = false;
    int c, r;

    while ((c = getopt(argc, argv, "Slv")) != -1) {
        switch (c) {
        case 'S':
            skip_filter = true;
            break;
        case 'l':
            do_list = true;
            break;
        case 'v':
            verbose ++;
            break;
        default:
            exit(usage(argv[0]));
        }
    }

    if (do_list) {
        return list_group_tests(um_group_name,
                                um_group_tests,
                                um_group_n_tests);
    }

    if (argc > optind)
        filter = argv[optind++];

    if (argc > optind)
        exit(usage(argv[0]));

    if (filter) {
        if (skip_filter)
            cmocka_set_skip_filter(filter);
        else
            cmocka_set_test_filter(filter);
    }

    if ((outfmt = getenv("CMOCKA_MESSAGE_OUTPUT"))
        && 0 == strcasecmp(outfmt, "subunit"))
    {
        print_message("progress: %u\n", (unsigned) um_group_n_tests);
    }

    r = _cmocka_run_group_tests(um_group_name,
                                um_group_tests,
                                um_group_n_tests,
                                um_group_setup,
                                um_group_teardown);

    return r;
}

static int list_group_tests(const char *const name,
                            const struct CMUnitTest tests[],
                            const size_t n_tests)
{
    unsigned i;

    for (i = 0; i < n_tests; i++) {
        printf("%s.%s\n", name, tests[i].name);
    }

    return 0;
}

void my_assert_float_equal(float a, float b,
                           const char *const file, const int line)
{
    const char msgprefix[] = "        --->";

    bool equalish = floats_equalish(a, b, 128 * FLT_EPSILON, FLT_MIN);

    if (!equalish) {
        cm_print_error("%.8g != %.8g (difference: %.8g)\n",
                       a, b, fabsf(a - b));
        _fail(file, line);
    }
    else if (verbose && a != b) {
        print_message("%s Dissimilar: %14.8g != %-14.8g (diff: %-.8f)\n",
                      msgprefix, a, b, fabs(a-b));
    }
}

void my_assert_float_not_equal(float a, float b,
                               const char *const file, const int line)
{
    bool equalish = floats_equalish(a, b, 128 * FLT_EPSILON, FLT_MIN);

    if (equalish) {
        cm_print_error("%.8g and %.8g are equalish (difference: %.8g)\n",
                       a, b, fabsf(a - b));
        _fail(file, line);
    }
}

/* cmocka 2.0 has this, but cmocka 1.x only has "assert_in_range" which is
 * unsigned
 */
void my_assert_int_in_range(const intmax_t value,
                            const intmax_t range_min,
                            const intmax_t range_max,
                            const char *const file,
                            const int line)
{
    bool in_range = value >= range_min && value <= range_max;

    if (!in_range) {
        cm_print_error("%" PRIiMAX "d is not within the range [%" PRIiMAX "d, "
                       "%" PRIiMAX "d]\n",
                       value, range_min, range_max);
        _fail(file, line);
    }
}

/* XXX long double would be nice here but mingw can't printf it */
void my_assert_float_in_range(const double value,
                              const double range_min,
                              const double range_max,
                              const char *const file,
                              const int line)
{
    bool in_range = value >= range_min && value <= range_max;

    if (!in_range) {
        cm_print_error("%.*g is not within the range [%.*g, %.*g]\n",
                       DBL_DECIMAL_DIG, value,
                       DBL_DECIMAL_DIG, range_min,
                       DBL_DECIMAL_DIG, range_max);
        _fail(file, line);
    }
}

void my_assert_string_prefix_equal(const char *a,
                                   const char *b,
                                   int len,
                                   const char *const file,
                                   const int line)
{
    if (0 != strncmp(a, b, len)) {
        cm_print_error("\"%.*s[...]\" is not equal to \"%.*s[...]\"",
                       len, a, len, b);
        _fail(file, line);
    }
}
