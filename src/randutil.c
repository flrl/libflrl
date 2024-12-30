#include "flrl/randutil.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef UNIT_TESTING
#undef assert
#define assert(ignore) ((void) 0)
#endif

/* XXX these should be somewhere else... */
#define MAX(a,b) ({         \
    __auto_type _a = (a);   \
    __auto_type _b = (b);   \
    _a > _b ? _a : _b;      \
})
#define MIN(a,b) ({         \
    __auto_type _a = (a);   \
    __auto_type _b = (b);   \
    _a < _b ? _a : _b;      \
})

struct bitstream {
    const struct rng *rng;
    uint64_t bits;
    unsigned n_bits;
};
#define BITSTREAM_INITIALIZER(g) (struct bitstream){ .rng = (g) }

static inline uint32_t mask_bits(unsigned bits)
{
    return bits < 32
           ? (UINT32_C(1) << bits) - 1
           : UINT32_C(0) - 1;
}

static uint32_t bs_bits(struct bitstream *bs, unsigned want_bits)
{
    uint32_t bits;

    assert(want_bits > 0 && want_bits <= 32);
    if (!want_bits) return 0;
    if (want_bits > 32) abort();

    if (bs->n_bits < want_bits) {
        uint64_t v = bs->rng->func(bs->rng->state);

        bs->bits |= (v << bs->n_bits);
        bs->n_bits += 32;
    }

    bits = bs->bits & mask_bits(want_bits);
    bs->bits >>= want_bits;
    bs->n_bits -= want_bits;

    return bits;
}

static unsigned bs_zeroes(struct bitstream *bs, unsigned limit)
{
    unsigned zeroes = 0, z;

    if (limit == 0 || limit > INT_MAX)
        limit = INT_MAX;

    while (limit) {
        if (bs->n_bits == 0) {
            bs->bits |= bs->rng->func(bs->rng->state);
            bs->n_bits += 32;
        }

        if (bs->bits == 0) {
            z = MIN(bs->n_bits, limit);
            zeroes += z;
            limit -= z;
            bs->bits >>= z;
            bs->n_bits -= z;
        }
        else {
            /* if the string of zeroes was stopped by a one, need to consume it
             * otherwise next bit is guaranteed to be one.  especially in the
             * case where there were no zeroes, in which case the state of the
             * rng won't advance if we don't consume at least one bit
             */
            bool saw_one = true;

            z = __builtin_ctzll(bs->bits);

            if (z >= limit) {
                saw_one = false;
                z = limit;
            }

            zeroes += z;
            bs->bits >>= z + saw_one;
            bs->n_bits -= z + saw_one;
            break;
        }
    }

    return zeroes;
}

extern inline int32_t randi32(const struct rng *rng, int32_t min, int32_t max);
extern inline int64_t randi64(const struct rng *rng, int64_t min, int64_t max);
extern inline uint32_t randu32(const struct rng *rng, uint32_t min, uint32_t max);
extern inline uint64_t randu64(const struct rng *rng, uint64_t min, uint64_t max);
extern inline float randf32(const struct rng *rng, double min, double max);
extern inline double randf64(const struct rng *rng, double min, double max);

void randi32v(const struct rng *rng,
              int32_t *out,
              size_t count,
              int32_t min,
              int32_t max)
{
    uint32_t range;
    size_t i;

    assert(min < max);
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

void randu32v(const struct rng *rng,
              uint32_t *out,
              size_t count,
              uint32_t min,
              uint32_t max)
{
    uint32_t range;
    size_t i;

    assert(min < max);
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

    assert(min < max);
    if (min > max) min = max;
    const double range = max - min;
    assert(range <= FLT_MAX);
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

    assert(min < max);
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

    assert(min < max);
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

    assert(min < max);
    if (min > max) min = max;
    const double range = max - min;
    assert(range <= DBL_MAX);
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

/* XXX wrandx functions for struct wrng */

/* XXX legacy */

uint32_t rand32_inrange(const struct rng *r,
                        uint32_t min, uint32_t max)
{
    uint32_t range;
    uint32_t value;
    int needloop;

    assert(max >= min);

    if (max == min) return min;

    range = 1 + max - min; /* inclusive */
    needloop = (0 != UINT32_MAX % range);

    do {
        value = r->func(r->state);
    } while (needloop && value > UINT32_MAX - range);

    return min + value % range;
}

uint64_t rand64_inrange(const struct wrng *r,
                        uint64_t min, uint64_t max)
{
    uint64_t range;
    uint64_t value;
    int needloop;

    assert(max >= min);

    if (max == min) return min;

    range = 1 + max - min; /* inclusive */
    needloop = (0 != UINT64_MAX % range);

    do {
        value = r->func(r->state);
    } while (needloop && value > UINT64_MAX - range);

    return min + value % range;
}

float rand32f_uniform(const struct rng *r)
{
    /* XXX naive: reasonably uniform, but only 83886081 possible values */
    return r->func(r->state) * 1.0 / UINT32_MAX;
}

double rand64f_uniform(const struct wrng *r)
{
    /* XXX naive approach, not actually uniform! */
    return r->func(r->state) * 1.0 / UINT64_MAX;
}

extern inline int rand32_coin(const struct rng *r, float p_heads);

extern inline int rand64_coin(const struct wrng *r, double p_heads);

unsigned sample32(const struct rng *r,
                  const unsigned weights[], size_t n_weights)
{
    unsigned *cdf = NULL;
    unsigned sum = 0;
    uint32_t rand;
    size_t i;

    assert(n_weights > 0);
    if (n_weights <= 1) return 0;

    cdf = malloc(n_weights * sizeof(*cdf));
    if (!cdf) return 0;

    for (i = 0; i < n_weights; i++) {
        sum += weights[i];
        assert(i == 0 || sum >= cdf[i - 1]); /* overflow detection */
        cdf[i] = sum;
    }

    assert(sum > 0);
    if (sum == 0) return 0;

    rand = rand32_inrange(r, 0, sum - 1);
    for (i = 0; i < n_weights && rand >= cdf[i]; i++)
        ;

    free(cdf);
    return i;
}

unsigned sample32v(const struct rng *r,
                   size_t n_pairs, ...)
{
    unsigned *values = NULL;
    unsigned *cdf = NULL;
    unsigned sum = 0;
    unsigned ret;
    uint32_t rand;
    va_list ap;
    size_t i;

    assert(n_pairs > 0);
    if (n_pairs == 0) return 0;

    values = malloc(n_pairs * sizeof(*values));
    cdf = malloc(n_pairs * sizeof(*cdf));
    if (!values || !cdf) {
        free(values);
        free(cdf);
        return 0;
    }

    va_start(ap, n_pairs);
    for (i = 0; i < n_pairs; i++) {
        unsigned weight = va_arg(ap, unsigned);
        unsigned value = va_arg(ap, unsigned);
        sum += weight;
        assert(i == 0 || sum >= cdf[i - 1]); /* overflow detection */
        values[i] = value;
        cdf[i] = sum;
    }
    va_end(ap);

    assert(sum > 0);
    if (sum == 0) return 0;

    rand = rand32_inrange(r, 0, sum - 1);
    for (i = 0; i < n_pairs && rand >= cdf[i]; i++)
        ;

    ret = values[i];
    free(values);
    free(cdf);

    return ret;
}

/* n elems of size z with struct weight at offset t, save cdf, return index */
unsigned sample32p(const struct rng *r,
                   void *data, size_t rows, size_t rowsize,
                   size_t weight_offset)
{
    void *p;
    struct weight *wp, *prev;
    uint16_t sum = 0;
    uint32_t rand;
    size_t i;

    assert(rows > 0);
    assert(rowsize > 0);

    /* lazy load cdf when last cumulative value is zero */
    wp = (struct weight *)((data + (rows - 1) * rowsize) + weight_offset);
    if (wp->cumulative == 0) {
        prev = NULL;
        for (p = data; p < (data + rows * rowsize); p += rowsize) {
            wp = (struct weight *)(p + weight_offset);
            sum += wp->weight;
            assert(prev == NULL || sum >= prev->cumulative); /* overflow */
            (void) prev; /* XXX shush 'prev unused' with assertions off */
            wp->cumulative = sum;
            prev = wp;
        }
    }
    else {
        sum = wp->cumulative;
    }
    assert(sum > 0);
    if (sum == 0) return 0;

    rand = rand32_inrange(r, 0, sum - 1);

    i = 0;
    do {
        wp = (struct weight *)((data + i * rowsize) + weight_offset);

        if (rand < wp->cumulative) break;

        i++;
    } while (i < rows);

    return i;
}

/* n weights, build temp cdf, return index */
extern unsigned sample64(const struct wrng *r,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample64v(const struct wrng *r,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample64p(const struct wrng *r,
                          void *data, size_t rows, size_t rowsize,
                          size_t weight_offset);

double rand64f_gaussian(const struct wrng *r, double mean, double stdev)
{
    static double v1, v2, s;
    static int phase = 0;
    double x;

    if (0 == phase) {
        do {
            double u1 = rand64f_uniform(r);
            double u2 = rand64f_uniform(r);

            v1 = 2 * u1 - 1;
            v2 = 2 * u2 - 1;
            s = v1 * v1 + v2 * v2;
        } while (s >= 1 || s == 0);

        x = v1 * sqrt(-2 * log(s) / s);
    }
    else {
        x = v2 * sqrt(-2 * log(s) / s);
    }

    phase = 1 - phase;

    return x * stdev + mean;
}
