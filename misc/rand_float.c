#include "flrl/randutil.h"
#include "flrl/xoshiro.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

uint32_t get_bit(const struct rng *rng)
{
    static uint32_t n_saved_bits = 0;
    static uint32_t saved_bits;
    uint32_t bit;

    if (n_saved_bits == 0) {
        saved_bits = rng->func(rng->state);
        n_saved_bits = 32;
    }

    bit = saved_bits & 1;
    saved_bits = saved_bits >> 1;
    n_saved_bits --;

    return bit;
}

float downey_randf(const struct rng *rng)
{
    int mant, exp, high_exp, low_exp;
    union {
        float f;
        int32_t i;
    } low, high, ans;

    low.f = 0.0; // XXX min arg?
    high.f = 1.0; // XXX max arg?

    /* extract the exponent fields from low and high */
    low_exp = (low.i >> 23) & 0xff;
    high_exp = (high.i >> 23) & 0xff;

    /* choose random bits and decrement exp until a 1 appears.
     * start at high_exp - 1 to leave room to maybe +1 later. */
    for (exp = high_exp - 1; exp > low_exp; exp--) {
        if (get_bit(rng)) break;
    }

    /* choose a random 23-bit mantissa */
    mant = rng->func(rng->state) & 0x7FFFFF;

    /* if the mantissa is zero, half the time we should move to the next
     * exponent range */
    if (mant == 0 && get_bit(rng)) exp++;

    /* combine the exponent and the mantissa */
    ans.i = (exp << 23) | mant;

    return ans.f;
}

void count_in_range(float low, float high)
{
    float x;
    uint64_t count = 0;

    for (x = low; x <= high; x = nextafterf(x, INFINITY))
        count++;

    printf("[%#.*g, %#.*g] possible values: %" PRIu64 "\n",
           FLT_DECIMAL_DIG, low,
           FLT_DECIMAL_DIG, high,
           count);
}

void count_distinct_converted32(float low, float high)
{
    const double mult = 1.0 / UINT32_MAX * (high - low);
    uint32_t i = 0, count = 0;
    float curr, prev = nanf("");

    do {
        curr = low + mult * i;
        if (curr != prev) {
            if (count == UINT32_MAX) abort();
            count ++;
            prev = curr;
        }
        i ++;
    } while (i != 0);

    printf("[%#.*g, %#.*g] 32 bit rng naively converts to %" PRIu32 " possible floats\n",
           FLT_DECIMAL_DIG, low,
           FLT_DECIMAL_DIG, high,
           count);
}

void count_distinct_converted64(void)
{
    const double mult = 1.0 / UINT64_MAX;
    uint64_t i = 0, count = 0;
    double curr, prev = nan("");

    abort(); /* XXX this will take a very long time! */
    /* https://www.reddit.com/r/ProgrammerTIL/comments/4tspsn/c_it_will_take_approximately_97_years_to_overflow/
     * like... 97 years lmao
     */

    do {
        curr = mult * i;
        if (curr != prev) {
            count ++;
            prev = curr;
        }
        i++;
    } while (i != 0);

    printf("64 bit rng naively converts to %" PRIu64 " possible doubles\n", count);
}

int main(int argc, char **argv)
{
    int opt;
    bool do_counts = false;
    bool do_downey = false;
    bool do_randutil = false;
    double low = 0.0, high = 1.0;

    while (-1 != (opt = getopt(argc, argv, "cdh:l:r"))) {
        switch (opt) {
        case 'c':
            do_counts = true;
            break;
        case 'd':
            do_downey = true;
            break;
        case 'h':
            high = atof(optarg);
            break;
        case 'l':
            low = atof(optarg);
            break;
        case 'r':
            do_randutil = true;
            break;
        default:
            return -1;
        }
    }

    assert(low < high);

    if (do_counts) {
        count_in_range(low, high);
        count_distinct_converted32(low, high);
//         count_distinct_converted64();
    }


    if (do_downey || do_randutil) {
        struct rng rng = RNG_INIT_XOSHIRO128_PLUSPLUS;
        size_t n_values = 1 * 1000 * 1000;
        float *values;
        unsigned i;

        assert(do_downey != do_randutil); /* XXX can't do both */

        xoshiro128plusplus_seed64(rng.state, time(0));

        values = calloc(n_values, sizeof(values[0]));
        if (!values) abort();

        if (do_downey) {
            for (i = 0; i < n_values; i++) {
                values[i] = low + (high - low) * downey_randf(&rng);
            }
        }
        else {
            randf32v(&rng, values, n_values, low, high);
        }

        for (i = 0; i < n_values; i++) {
            printf("%.*g,%.*g\n",
                FLT_DECIMAL_DIG, values[i],
                FLT_DECIMAL_DIG, fabsf(values[i]));
        }

        free(values);
    }

    return 0;
}
