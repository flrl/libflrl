#include "flrl/randutil.h"
#include "flrl/xoshiro.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

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

void state128_seed(struct state128 *s, const void *seed, size_t len)
{
    /* XXX repeat pattern if len is short? */
    memmove(s, seed, MIN(len, sizeof(*s)));
}

void state128_seed64(struct state128 *s, uint64_t seed)
{
    xoshiro_seed64(s, sizeof(*s), seed);
}

extern inline void randbs_seed(struct randbs *bs, const void *seed, size_t len);
extern inline void randbs_seed64(struct randbs *bs, uint64_t seed);

static inline uint64_t mask_bits(unsigned bits)
{
    return bits < 64
           ? (UINT64_C(1) << bits) - 1
           : UINT64_C(0) - 1;
}

uint64_t randbs_bits(struct randbs *bs, unsigned want_bits)
{
    uint64_t bits;
    uint32_t extra = 0;

    assert(want_bits > 0 && want_bits <= RANDBS_MAX_BITS);
    if (!want_bits) return 0;
    if (want_bits > RANDBS_MAX_BITS) abort();

    while (bs->n_bits < want_bits) {
        uint64_t v;
        unsigned b;

        v = bs->rng.func(bs->rng.state);
        b = RANDBS_MAX_BITS - bs->n_bits;

        bs->bits |= v << bs->n_bits;
        extra = b < 32 ? v >> b : 0;
        bs->n_bits += 32;
    }

    bits = bs->bits & mask_bits(want_bits);

    bs->bits = want_bits < RANDBS_MAX_BITS ? bs->bits >> want_bits : 0;
    if (extra)
        bs->bits |= (uint64_t) extra << (RANDBS_MAX_BITS - want_bits);
    bs->n_bits -= want_bits;

    return bits;
}

unsigned randbs_zeroes(struct randbs *bs, unsigned limit)
{
    unsigned zeroes = 0, z;

    if (limit == 0 || limit > INT_MAX)
        limit = INT_MAX;

    while (limit) {
        if (bs->n_bits == 0) {
            bs->bits |= bs->rng.func(bs->rng.state);
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

extern inline int32_t randi32(struct randbs *bs, int32_t min, int32_t max);
extern inline int64_t randi64(struct randbs *bs, int64_t min, int64_t max);
extern inline uint32_t randu32(struct randbs *bs, uint32_t min, uint32_t max);
extern inline uint64_t randu64(struct randbs *bs, uint64_t min, uint64_t max);
extern inline float randf32(struct randbs *bs, double min, double max);
extern inline double randf64(struct randbs *bs, double min, double max);
extern inline bool coin(struct randbs *bs, float p_true);

/* XXX wrandx functions for struct wrng */

/* XXX legacy */

unsigned sample32(struct randbs *bs,
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

    rand = randu32(bs, 0, sum - 1);
    for (i = 0; i < n_weights && rand >= cdf[i]; i++)
        ;

    free(cdf);
    return i;
}

unsigned sample32v(struct randbs *bs,
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

    rand = randu32(bs, 0, sum - 1);
    for (i = 0; i < n_pairs && rand >= cdf[i]; i++)
        ;

    ret = values[i];
    free(values);
    free(cdf);

    return ret;
}

/* n elems of size z with struct weight at offset t, save cdf, return index */
unsigned sample32p(struct randbs *bs,
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

    rand = randu32(bs, 0, sum - 1);

    i = 0;
    do {
        wp = (struct weight *)((data + i * rowsize) + weight_offset);

        if (rand < wp->cumulative) break;

        i++;
    } while (i < rows);

    return i;
}
