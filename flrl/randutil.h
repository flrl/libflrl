#ifndef LIBFLRL_RANDUTIL_H
#define LIBFLRL_RANDUTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
 #define restrict
 using std::size_t;
 extern double randutil_fma(double x, double y, double z);
 extern double randutil_log(double x);
 extern double randutil_sqrt(double x);
#endif

struct state128 {
    uint32_t s[4];
};

struct state256 {
    uint64_t s[4];
};

extern void state_seed(void *restrict state, size_t state_len,
                       const void *restrict seed, size_t seed_len);
extern void state_seed_sm64(void *state, size_t state_len, uint64_t seed);

inline void state128_seed(struct state128 *restrict state,
                          const struct state128 *restrict seed)
{
    state_seed(state, sizeof(*state), seed, sizeof(*seed));
}

inline void state128_seed64(struct state128 *state, uint64_t seed)
{
    state_seed_sm64(state, sizeof(*state), seed);
}

inline void state256_seed(struct state256 *restrict state,
                          const struct state256 *restrict seed)
{
    state_seed(state, sizeof(*state), seed, sizeof(*seed));
}

inline void state256_seed64(struct state256 *state, uint64_t seed)
{
    state_seed_sm64(state, sizeof(*state), seed);
}

struct randbs {
    struct state128 state;
    uint32_t (*func)(struct state128 *);
    uint64_t bits;
    unsigned n_bits;
} __attribute__((aligned(64)));
#define RANDBS_INITIALIZER(f) (struct randbs){ .func = f }
#define RANDBS_MAX_BITS (64U)

inline void randbs_seed(struct randbs *bs, const struct state128 *seed)
{
    state128_seed(&bs->state, seed);
    bs->bits = bs->n_bits = 0;
}

inline void randbs_seed64(struct randbs *bs, uint64_t seed)
{
    state128_seed64(&bs->state, seed);
    bs->bits = bs->n_bits = 0;
}

extern uint64_t randbs_bits(struct randbs *bs, unsigned want_bits);
extern unsigned randbs_zeroes(struct randbs *bs, unsigned limit);

struct wrandbs {
    struct state256 state;
    uint64_t (*func)(struct state256 *);
    uint64_t bits;
    unsigned n_bits;
} __attribute__((aligned(64)));
#define WRANDBS_INITIALIZER(f) (struct wrandbs){ .func = f }
#define WRANDBS_MAX_BITS (64U)

inline void wrandbs_seed(struct wrandbs *bs, const struct state256 *seed)
{
    state256_seed(&bs->state, seed);
    bs->bits = bs->n_bits = 0;
}

inline void wrandbs_seed64(struct wrandbs *bs, uint64_t seed)
{
    state256_seed64(&bs->state, seed);
    bs->bits = bs->n_bits = 0;
}

extern uint64_t wrandbs_bits(struct wrandbs *bs, unsigned want_bits);
extern unsigned wrandbs_zeroes(struct wrandbs *bs, unsigned limit);

extern void randi8v(struct randbs *bs,
                    int8_t *out,
                    size_t count,
                    int8_t min,
                    int8_t max);
extern void randi16v(struct randbs *bs,
                     int16_t *out,
                     size_t count,
                     int16_t min,
                     int16_t max);
extern void randi32v(struct randbs *bs,
                     int32_t *out,
                     size_t count,
                     int32_t min,
                     int32_t max);
extern void randi64v(struct randbs *bs,
                     int64_t *out,
                     size_t count,
                     int64_t min,
                     int64_t max);

extern void randu8v(struct randbs *bs,
                    uint8_t *out,
                    size_t count,
                    uint8_t min,
                    uint8_t max);
extern void randu16v(struct randbs *bs,
                     uint16_t *out,
                     size_t count,
                     uint16_t min,
                     uint16_t max);
extern void randu32v(struct randbs *bs,
                     uint32_t *out,
                     size_t count,
                     uint32_t min,
                     uint32_t max);
extern void randu64v(struct randbs *bs,
                     uint64_t *out,
                     size_t count,
                     uint64_t min,
                     uint64_t max);

extern void randf32v(struct randbs *bs,
                     float *out,
                     size_t count,
                     double min,
                     double max);
extern void randf64v(struct randbs *bs,
                     double *out,
                     size_t count,
                     double min,
                     double max);

inline int8_t randi8(struct randbs *bs, int8_t min, int8_t max)
{
    int8_t v;
    randi8v(bs, &v, 1, min, max);
    return v;
}

inline int16_t randi16(struct randbs *bs, int16_t min, int16_t max)
{
    int16_t v;
    randi16v(bs, &v, 1, min, max);
    return v;
}

inline int32_t randi32(struct randbs *bs, int32_t min, int32_t max)
{
    int32_t v;
    randi32v(bs, &v, 1, min, max);
    return v;
}

inline int64_t randi64(struct randbs *bs, int64_t min, int64_t max)
{
    int64_t v;
    randi64v(bs, &v, 1, min, max);
    return v;
}

inline uint8_t randu8(struct randbs *bs, uint8_t min, uint8_t max)
{
    uint8_t v;
    randu8v(bs, &v, 1, min, max);
    return v;
}

inline uint16_t randu16(struct randbs *bs, uint16_t min, uint16_t max)
{
    uint16_t v;
    randu16v(bs, &v, 1, min, max);
    return v;
}

inline uint32_t randu32(struct randbs *bs, uint32_t min, uint32_t max)
{
    uint32_t v;
    randu32v(bs, &v, 1, min, max);
    return v;
}

inline uint64_t randu64(struct randbs *bs, uint64_t min, uint64_t max)
{
    uint64_t v;
    randu64v(bs, &v, 1, min, max);
    return v;
}

inline float randf32(struct randbs *bs, double min, double max)
{
    float v;
    randf32v(bs, &v, 1, min, max);
    return v;
}

inline double randf64(struct randbs *bs, double min, double max)
{
    double v;
    randf64v(bs, &v, 1, min, max);
    return v;
}

extern void gaussf32v(struct randbs *bs,
                      float *out,
                      size_t count,
                      double mean,
                      double stddev);

extern float gaussf32(struct randbs *bs, double mean, double stddev);

extern void gaussf64v(struct randbs *bs,
                      double *out,
                      size_t count,
                      double mean,
                      double stddev);

extern double gaussf64(struct randbs *bs, double mean, double stddev);

inline bool coin(struct randbs *bs, float p_true)
{
    return randf32(bs, 0.0, 1.0) <= p_true;
}

extern void wrandi32v(struct wrandbs *bs,
                      int32_t *out,
                      size_t count,
                      int32_t min,
                      int32_t max);
extern void wrandi64v(struct wrandbs *bs,
                      int64_t *out,
                      size_t count,
                      int64_t min,
                      int64_t max);
extern void wrandu32v(struct wrandbs *bs,
                      uint32_t *out,
                      size_t count,
                      uint32_t min,
                      uint32_t max);
extern void wrandu64v(struct wrandbs *bs,
                      uint64_t *out,
                      size_t count,
                      uint64_t min,
                      uint64_t max);
extern void wrandf32v(struct wrandbs *bs,
                      float *out,
                      size_t count,
                      double min,
                      double max);
extern void wrandf64v(struct wrandbs *bs,
                      double *out,
                      size_t count,
                      double min,
                      double max);

inline int32_t wrandi32(struct wrandbs *bs, int32_t min, int32_t max)
{
    int32_t v;
    wrandi32v(bs, &v, 1, min, max);
    return v;
}

inline int64_t wrandi64(struct wrandbs *bs, int64_t min, int64_t max)
{
    int64_t v;
    wrandi64v(bs, &v, 1, min, max);
    return v;
}

inline uint32_t wrandu32(struct wrandbs *bs, uint32_t min, uint32_t max)
{
    uint32_t v;
    wrandu32v(bs, &v, 1, min, max);
    return v;
}

inline uint64_t wrandu64(struct wrandbs *bs, uint64_t min, uint64_t max)
{
    uint64_t v;
    wrandu64v(bs, &v, 1, min, max);
    return v;
}

inline float wrandf32(struct wrandbs *bs, double min, double max)
{
    float v;
    wrandf32v(bs, &v, 1, min, max);
    return v;
}

inline double wrandf64(struct wrandbs *bs, double min, double max)
{
    double v;
    wrandf64v(bs, &v, 1, min, max);
    return v;
}

extern void wgaussf32v(struct wrandbs *bs,
                       float *out,
                       size_t count,
                       double mean,
                       double stddev);

extern float wgaussf32(struct wrandbs *bs, double mean, double stddev);

extern void wgaussf64v(struct wrandbs *bs,
                       double *out,
                       size_t count,
                       double mean,
                       double stddev);

extern double wgaussf64(struct wrandbs *bs, double mean, double stddev);

inline bool wcoin(struct wrandbs *bs, float p_true)
{
    return wrandf32(bs, 0.0, 1.0) <= p_true;
}

/* XXX legacy */
struct weight {
    uint16_t weight;
    uint16_t cumulative;
};

/* n weights, build temp cdf, return index */
extern unsigned sample32(struct randbs *bs,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample32v(struct randbs *bs,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample32p(struct randbs *bs,
                          void *data, size_t rows, size_t rowsize,
                          size_t weight_offset);

#endif
