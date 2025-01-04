#include "flrl/randutil.h"
#include "flrl/splitmix64.h"

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
    struct splitmix64_state sm64;
    uint64_t buf[2];

    static_assert(sizeof(*s) == sizeof(buf));

    sm64.x = seed;
    buf[0] = splitmix64_next(&sm64);
    buf[1] = splitmix64_next(&sm64);

    memcpy(s, buf, sizeof(*s));
}

extern inline void randbs_seed(struct randbs *bs, const void *seed, size_t len);
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
