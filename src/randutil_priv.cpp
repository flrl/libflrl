extern "C" {
#include "flrl/randutil.h"
}

#include <bit>
#include <limits>
#include <type_traits>

template<typename R, typename T>
void randiv(const R *rng, T *out, std::size_t count, T min, T max)
{
    using UT = std::make_unsigned_t<T>;
    UT range;
    std::size_t i;

    if (min > max) min = max;

    range = max - min;

    if (range == 0) {
        for (i = 0; i < count; i++) {
            out[i] = min;
        }
    }
    else {
        struct bitstream bs = BITSTREAM_INITIALIZER(rng);
        unsigned want_bits = std::bit_width(range);

        /* XXX adjust want_bits for perverse cases? */

        for (i = 0; i < count; i++) {
            uint64_t v;

            do {
                v = bs_bits(&bs, want_bits);
            } while (v > range);

            out[i] = min + v;
        }
    }
}

extern "C" {

#include <assert.h>
#include <float.h>
#include <math.h>

void randi32v(const struct rng *rng,
              int32_t *out,
              size_t count,
              int32_t min,
              int32_t max)
{
    randiv<struct rng, int32_t>(rng, out, count, min, max);
}

void randu32v(const struct rng *rng,
              uint32_t *out,
              size_t count,
              uint32_t min,
              uint32_t max)
{
    uint32_t range;
    size_t i;

    if (min > max) min = max;

    range = max - min + 1; /* wraps to zero at max_range */

    if (range != 1) {
        uint32_t div = 1, limit = 0;

        if (range) {
            /* max_range should be UINT32_MAX + 1, but would need a wider type.
             * get around that with: a / b == 1 + (a - b) / b
             */
            uint32_t max_range = UINT32_MAX - range + 1;
            div = range > 1 ? 1 + max_range / range : 1;
            limit = range * div;
        }

        for (i = 0; i < count; i++) {
            uint32_t v;

            do {
                v = rng->func(rng->state);
            } while (limit && v >= limit);

            out[i] = v / div + min;
        }
    }
    else {
        for (i = 0; i < count; i++) {
            out[i] = min;
        }
    }
}

/* based on https://allendowney.com/research/rand/downey07randfloat.pdf */
void randf32v(const struct rng *rng,
              float *out,
              size_t count,
              double min,
              double max)
{
    struct bitstream bs = BITSTREAM_INITIALIZER(rng);
    union overlay { float f; uint32_t i; };
    size_t i;

    if (min > max) min = max;
    const double range = max - min;
    if (range > FLT_MAX) abort();

    /* we'll generate values in [0, 1], then scale and translate to [min,max] */
    const union overlay low = { .f = 0.0 };
    const union overlay high = { .f = 1.0 };
    const unsigned low_exp = (low.i >> 23) & 0xff;
    const unsigned high_exp = ((high.i >> 23) & 0xff) - 1;
    assert(high_exp > low_exp);

    for (i = 0; i < count; i++) {
        uint32_t mantissa, exponent;
        union overlay val;

        /* choose random bits and decrement exponent until a 1 appears.
         * start at high_exp - 1 to leave room to maybe +1 later. */
        exponent = high_exp - bs_zeroes(&bs, high_exp - low_exp);

        /* choose a random 23-bit mantissa */
        mantissa = bs_bits(&bs, 23);

        /* if the mantissa is zero, half the time we should move to the next
         * exponent range */
        if (mantissa == 0 && bs_bits(&bs, 1))
            exponent ++;

        /* combine the exponent and the mantissa */
        val.i = (exponent << 23) | mantissa;

        out[i] = fma(range, val.f, min);
    }
}

void randi64v(const struct rng *rng,
              int64_t *out,
              size_t count,
              int64_t min,
              int64_t max)
{
    uint64_t range;
    size_t i;

    if (min > max) min = max;

    range = max - min + 1; /* wraps to zero at max_range */

    if (range != 1) {
        uint64_t div = 1, limit = 0;

        if (range) {
            /* max_range should be UINT64_MAX + 1, but would need a wider type.
             * get around that with: a / b == 1 + (a - b) / b
             */
            uint64_t max_range = UINT64_MAX - range + 1;
            div = range > 1 ? 1 + max_range / range : 1;
            limit = range * div;
        }

        for (i = 0; i < count; i++) {
            uint64_t v;

            do {
                /* to get 64 bits we need two samples from this generator */
                uint64_t v0, v1;

                /* XXX only sample once if we don't need all that range? */
                v0 = rng->func(rng->state);
                v1 = rng->func(rng->state);

                v = v0 << 32 | v1;
            } while (limit && v >= limit);

            out[i] = v / div + min;
        }
    }
    else {
        for (i = 0; i < count; i++) {
            out[i] = min;
        }
    }
}

void randu64v(const struct rng *rng,
              uint64_t *out,
              size_t count,
              uint64_t min,
              uint64_t max)
{
    uint64_t range;
    size_t i;

    if (min > max) min = max;

    range = max - min + 1; /* wraps to zero at max_range */

    if (range != 1) {
        uint64_t div = 1, limit = 0;

        if (range) {
            /* max_range should be UINT64_MAX + 1, but would need a wider type.
             * get around that with: a / b == 1 + (a - b) / b
             */
            uint64_t max_range = UINT64_MAX - range + 1;
            div = range > 1 ? 1 + max_range / range : 1;
            limit = range * div;
        }

        for (i = 0; i < count; i++) {
            uint64_t v;

            do {
                /* to get 64 bits we need two samples from this generator */
                uint64_t v0, v1;

                /* XXX only sample once if we don't need all that range? */
                v0 = rng->func(rng->state);
                v1 = rng->func(rng->state);

                v = v0 << 32 | v1;
            } while (limit && v >= limit);

            out[i] = v / div + min;
        }
    }
    else {
        for (i = 0; i < count; i++) {
            out[i] = min;
        }
    }
}

/* based on https://allendowney.com/research/rand/downey07randfloat.pdf */
void randf64v(const struct rng *rng,
              double *out,
              size_t count,
              double min,
              double max)
{
    struct bitstream bs = BITSTREAM_INITIALIZER(rng);
    union overlay { double f; uint64_t i; };
    size_t i;

    if (min > max) min = max;
    const double range = max - min;
    if (range > DBL_MAX) abort();

    /* we'll generate values in [0, 1], then scale and translate to [min,max] */
    const union overlay low = { .f = 0.0 };
    const union overlay high = { .f = 1.0 };
    const unsigned low_exp = (low.i >> 52) & 0x7ff;
    const unsigned high_exp = ((high.i >> 52) & 0x7ff) - 1;
    assert(high_exp > low_exp);

    for (i = 0; i < count; i++) {
        uint64_t mantissa, exponent;
        union overlay val;

        /* choose random bits and decrement exponent until a 1 appears.
         * start at high_exp - 1 to leave room to maybe +1 later. */
        exponent = high_exp - bs_zeroes(&bs, high_exp - low_exp);

        /* choose a random 52-bit mantissa */
        mantissa = (uint64_t) bs_bits(&bs, 20) << 32 | bs_bits(&bs, 32);

        /* if the mantissa is zero, half the time we should move to the next
         * exponent range */
        if (mantissa == 0 && bs_bits(&bs, 1))
            exponent ++;

        /* combine the exponent and the mantissa */
        val.i = (exponent << 52) | mantissa;

        out[i] = fma(range, val.f, min);
    }
}

void gaussf32v(const struct rng *rng,
               float *out,
               size_t count,
               double mean,
               double stddev)
{
    size_t i;

    for (i = 0; i < count; i += 2) {
        double v[2], s, t;

        do {
            float u[2];

            randf32v(rng, u, 2, 0.0, 1.0);

            v[0] = fma(2.0, u[0], -1.0);
            v[1] = fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = sqrt(-2.0 * log(s) / s);

        out[i] = fma(stddev, v[0] * t, mean);
        if (i + 1 < count)
            out[i + 1] = fma(stddev, v[1] * t, mean);
    }
}

float gaussf32(const struct rng *rng, double mean, double stddev)
{
    static double v[2], t;
    static int phase = 0;
    double x;

    if (0 == phase) {
        double s;

        do {
            float u[2];

            randf32v(rng, u, 2, 0.0, 1.0);

            v[0] = fma(2.0, u[0], -1.0);
            v[1] = fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = sqrt(-2.0 * log(s) / s);
        x = v[0] * t;
    }
    else {
        x = v[1] * t;
    }

    phase = 1 - phase;

    return fma(stddev, x, mean);
}

void gaussf64v(const struct rng *rng,
               double *out,
               size_t count,
               double mean,
               double stddev)
{
    size_t i;

    for (i = 0; i < count; i += 2) {
        double v[2], s, t;

        do {
            double u[2];

            randf64v(rng, u, 2, 0.0, 1.0);

            v[0] = fma(2.0, u[0], -1.0);
            v[1] = fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = sqrt(-2.0 * log(s) / s);

        out[i] = fma(stddev, v[0] * t, mean);
        if (i + 1 < count)
            out[i + 1] = fma(stddev, v[1] * t, mean);
    }
}

double gaussf64(const struct rng *rng, double mean, double stddev)
{
    static double v[2], t;
    static int phase = 0;
    double x;

    if (0 == phase) {
        double s;

        do {
            double u[2];

            randf64v(rng, u, 2, 0.0, 1.0);

            v[0] = fma(2.0, u[0], -1.0);
            v[1] = fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = sqrt(-2.0 * log(s) / s);
        x = v[0] * t;
    }
    else {
        x = v[1] * t;
    }

    phase = 1 - phase;

    return fma(stddev, x, mean);
}

}