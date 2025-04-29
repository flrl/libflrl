#include "unitmain.h"

#include "flrl/base64.h"
#include "flrl/fputil.h"
#include "flrl/randutil.h"
#include "flrl/xoshiro.h"

#include <assert.h>
#include <float.h>
#include <getopt.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

int verbose;
uint64_t um_seed = 0;
char um_encoded_seed[16] = {0};

static int setup_seed(const char *seedstr);
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
    const char *seed = NULL;
    bool skip_filter = false;
    bool do_list = false;
    int c, r;

    setlocale(LC_ALL, ".utf8");

    while ((c = getopt(argc, argv, "Sls:v")) != -1) {
        switch (c) {
        case 'S':
            skip_filter = true;
            break;
        case 'l':
            do_list = true;
            break;
        case 's':
            seed = optarg;
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

    if (setup_seed(seed))
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

/* arrange for same seed to be used by each test, and saved to a file
 * so that if a randomised test fails, it can be rerun in the debugger
 * using the same seed to find out why
 */
static int setup_seed(const char *seedstr)
{
    char seed_fname[64];
    FILE *f;
    int r;
    bool got_seed = false;

    snprintf(seed_fname, sizeof(seed_fname), "test/%s.seed", um_group_name);

    if (seedstr && *seedstr) {
        um_seed = 0;
        r = base64_decode(&um_seed, sizeof(um_seed), seedstr, 0);
        if (r < 0) {
            fprintf(stderr, "base64_decode(%s) returned %lld (%llx)\n",
                            seedstr, (int64_t) r, um_seed);
            fprintf(stderr, "invalid seed: %s\n", seedstr);
            fputs("    valid characters are A-Z, a-z, 0-9, '-', and '_'\n", stderr);
            fputs("    maximum length 11 chars\n", stderr);
            fflush(stderr);
            return r;
        }
        got_seed = true;
    }

    if (!got_seed && (f = fopen(seed_fname, "r"))) {
        char buf[128];

        if (fgets(buf, sizeof(buf), f)) {
            um_seed = 0;
            r = base64_decode(&um_seed, sizeof(um_seed), buf, 0);
            if (r > 0)
                got_seed = true;
        }
        fclose(f);
    }

    if (!got_seed) {
        um_seed = time(NULL);
    }

    base64_encode(um_encoded_seed, sizeof(um_encoded_seed),
                  &um_seed, sizeof(um_seed));
    if (verbose) {
        fprintf(stderr, "%s: using seed: %s (%llx)\n",
                        um_group_name, um_encoded_seed, um_seed);
        fflush(stderr);
    }

    f = fopen(seed_fname, "w");
    if (!f) {
        perror(seed_fname);
        return -1;
    }
    else {
        fprintf(f, "%s\n", um_encoded_seed);
        fclose(f);
    }

    return 0;
}

int um_setup_rbs(void **state)
{
    static struct randbs rbs;

    rbs = RANDBS_INITIALIZER(&xoshiro128plusplus_next);
    randbs_seed64(&rbs, um_seed);

    *state = &rbs;
    return 0;
}

void my_assert_float_equal(float a, float b,
                           const char *const file, const int line)
{
    const char msgprefix[] = "        --->";

    bool equalish = floats_equalish(a, b, 128 * FLT_EPSILON, FLT_MIN);

    if (!equalish && !(isnan(a) && isnan(b))) {
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
