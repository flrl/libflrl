#include "flrl/randutil.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef UNIT_TESTING
#undef assert
#define assert(ignore) ((void) 0)
#endif

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

void randi64v(const struct rng *rng __attribute__((unused)),
              int64_t *out __attribute__((unused)),
              size_t count __attribute__((unused)),
              int64_t min __attribute__((unused)),
              int64_t max __attribute__((unused)))
{
    abort();
}

void randu32v(const struct rng *rng,
              uint32_t *out,
              size_t count,
              uint32_t min,
              uint32_t max)
{
    const uint64_t max_range = UINT64_C(1) + UINT32_MAX;
    uint32_t range, div, limit;
    size_t i;

    assert(min < max);
    if (min > max) min = max;

    range = max - min + 1; /* wraps to zero at max_range */
    div = range > 1 ? max_range / range :  1;
    limit = range ? range * div : 0;

    for (i = 0; i < count; i++) {
        uint32_t v;

        do {
            v = rng->func(rng->state);
        } while (limit && v >= limit);

        out[i] = v / div + min;
    }
}

void randu64v(const struct rng *rng __attribute__((unused)),
              uint64_t *out __attribute__((unused)),
              size_t count __attribute__((unused)),
              uint64_t min __attribute__((unused)),
              uint64_t max __attribute__((unused)))
{
    abort();
}

void randf32v(const struct rng *rng,
              float *out,
              size_t count,
              double min,
              double max)
{
    size_t i;

    assert(min < max);

    /* XXX this does not produce good uniform values! */
    for (i = 0; i < count; i++) {
        double scale = rng->func(rng->state) / (double) UINT32_MAX;
        out[i] = min + scale * (max - min);
    }
}

void randf64v(const struct rng *rng __attribute__((unused)),
              double *out __attribute__((unused)),
              size_t count __attribute__((unused)),
              double min __attribute__((unused)),
              double max __attribute__((unused)))
{
    abort();
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
    /* XXX naive approach, not actually uniform! */
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

float rand32f_gaussian(const struct rng *r, float mean, float stdev)
{
    static float v1, v2, s;
    static int phase = 0;
    float x;

    if (0 == phase) {
        do {
            float u1 = rand32f_uniform(r);
            float u2 = rand32f_uniform(r);

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
