#include "flrl/randutil.h"

#include "flrl/splitmix64.h"
#include "flrl/xassert.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

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

double randutil_fma(double x, double y, double z)
{
    return fma(x, y, z);
}

double randutil_log(double x)
{
    return log(x);
}

double randutil_sqrt(double x)
{
    return sqrt(x);
}

void state_seed(void *state, size_t state_len,
                const void *seed, size_t seed_len)
{
    /* XXX repeat pattern if len is short? */
    memmove(state, seed, MIN(state_len, seed_len));
}

void state_seed_sm64(void *state, size_t state_len, uint64_t seed)
{
    const size_t n = (state_len + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    uint64_t buf[n];
    struct splitmix64_state sm64;
    size_t i;

    sm64.x = seed;
    for (i = 0; i < n; i++) {
        buf[i] = splitmix64_next(&sm64);
    }

    memcpy(state, buf, state_len);
}

void shuffle(struct randbs *rbs, void *base, size_t n_elems, size_t elem_size)
{
    unsigned char *array = base, *tmp;
    size_t i, j;

    tmp = malloc(elem_size);

    /* if malloc failed, order is unchanged, but that's a valid outcome */
    if (MALLOC_FAILED(!tmp)) return;

    for (i = 0; i < n_elems - 1; i++) {
        j = randu64(rbs, i, n_elems - 1);
        if (i != j) {
            memcpy(tmp, array + i * elem_size, elem_size);
            memcpy(array + i * elem_size, array + j * elem_size, elem_size);
            memcpy(array + j * elem_size, tmp, elem_size);
        }
    }

    free(tmp);
}

extern inline void state128_seed(struct state128 *restrict state,
                                 const struct state128 *seed);
extern inline void state128_seed64(struct state128 *state, uint64_t seed);

extern inline void randbs_seed(struct randbs *bs, const struct state128 *seed);
extern inline void randbs_seed64(struct randbs *bs, uint64_t seed);

extern inline int8_t randi8(struct randbs *bs, int8_t min, int8_t max);
extern inline int16_t randi16(struct randbs *bs, int16_t min, int16_t max);
extern inline int32_t randi32(struct randbs *bs, int32_t min, int32_t max);
extern inline int64_t randi64(struct randbs *bs, int64_t min, int64_t max);

extern inline uint8_t randu8(struct randbs *bs, uint8_t min, uint8_t max);
extern inline uint16_t randu16(struct randbs *bs, uint16_t min, uint16_t max);
extern inline uint32_t randu32(struct randbs *bs, uint32_t min, uint32_t max);
extern inline uint64_t randu64(struct randbs *bs, uint64_t min, uint64_t max);

extern inline float randf32(struct randbs *bs, double min, double max);
extern inline double randf64(struct randbs *bs, double min, double max);

extern inline bool coin(struct randbs *bs, float p_true);

#if 0
extern inline void wrandbs_seed(struct wrandbs *bs, const void *seed, size_t len);
extern inline void wrandbs_seed64(struct wrandbs *bs, uint64_t seed);

extern inline int32_t wrandi32(struct wrandbs *bs, int32_t min, int32_t max);
extern inline int64_t wrandi64(struct wrandbs *bs, int64_t min, int64_t max);
extern inline uint32_t wrandu32(struct wrandbs *bs, uint32_t min, uint32_t max);
extern inline uint64_t wrandu64(struct wrandbs *bs, uint64_t min, uint64_t max);
extern inline float wrandf32(struct wrandbs *bs, double min, double max);
extern inline double wrandf64(struct wrandbs *bs, double min, double max);
extern inline bool wcoin(struct wrandbs *bs, float p_true);
#endif

/* XXX legacy */

unsigned sample32(struct randbs *bs,
                  const unsigned weights[], size_t n_weights)
{
    unsigned *cdf = NULL;
    unsigned sum = 0;
    uint32_t rand;
    size_t i;

    if (!soft_assert(n_weights > 0)) return 0;

    cdf = malloc(n_weights * sizeof(*cdf));
    if (!cdf) return 0;

    for (i = 0; i < n_weights; i++) {
        sum += weights[i];
        hard_assert(i == 0 || sum >= cdf[i - 1]); /* overflow detection */
        cdf[i] = sum;
    }

    if (!soft_assert(sum > 0)) return 0;

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

    if (!soft_assert(n_pairs > 0)) return 0;

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
        hard_assert(i == 0 || sum >= cdf[i - 1]); /* overflow detection */
        values[i] = value;
        cdf[i] = sum;
    }
    va_end(ap);

    if (!soft_assert(sum > 0)) return 0;

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

    hard_assert(rows > 0);
    hard_assert(rowsize > 0);

    /* lazy load cdf when last cumulative value is zero */
    wp = (struct weight *)((data + (rows - 1) * rowsize) + weight_offset);
    if (wp->cumulative == 0) {
        prev = NULL;
        for (p = data; p < (data + rows * rowsize); p += rowsize) {
            wp = (struct weight *)(p + weight_offset);
            sum += wp->weight;
            hard_assert(prev == NULL || sum >= prev->cumulative); /* overflow */
            wp->cumulative = sum;
            prev = wp;
        }
    }
    else {
        sum = wp->cumulative;
    }

    if (!soft_assert(sum > 0)) return 0;

    rand = randu32(bs, 0, sum - 1);

    i = 0;
    do {
        wp = (struct weight *)((data + i * rowsize) + weight_offset);

        if (rand < wp->cumulative) break;

        i++;
    } while (i < rows);

    return i;
}
