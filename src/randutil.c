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

void init_cdf(unsigned *cdf,
              const void *base, size_t n_elems, size_t elem_size,
              size_t weight_offset)
{
    const uint8_t *data = base;
    unsigned i, sum = 0;

    hard_assert(elem_size >= sizeof(unsigned));

    for (i = 0; i < n_elems; i++) {
        const uint8_t *p = data + i * elem_size;
        unsigned w = *(const unsigned *)(p + weight_offset);

        sum += w;
        hard_assert(i == 0 || sum >= cdf[i - 1]); /* overflow detection */
        cdf[i] = sum;
    }
}

unsigned sample_cdf(struct randbs *bs,
                    const unsigned *cdf, size_t n_elems)
{
    uint32_t rand;
    unsigned i;

    rand = randu32(bs, 0, cdf[n_elems - 1] - 1);
    for (i = 0; i < n_elems && rand >= cdf[i]; i++)
        ;

    assert(i < n_elems);
    assert(rand < cdf[i]);
    return i;
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
