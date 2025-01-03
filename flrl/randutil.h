#ifndef LIBFLRL_RANDUTIL_H
#define LIBFLRL_RANDUTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct state128 {
    uint32_t s[4];
};

struct state256 {
    uint64_t s[4];
};

extern void state128_seed(struct state128 *s, const void *seed, size_t len);
extern void state128_seed64(struct state128 *s, uint64_t seed);

struct rng {
    struct state128 state;
    uint32_t (*func)(void *);
};

struct wrng {
    struct state256 state;
    uint64_t (*func)(void *);
};

struct randbs {
    struct rng rng;
    uint64_t bits;
    unsigned n_bits;
} __attribute__((aligned(64)));
#define RANDBS_INITIALIZER(g) (struct randbs){ (g), 0, 0 }
#define RANDBS_MAX_BITS (64U)

inline void randbs_seed(struct randbs *bs, const void *seed, size_t len)
{
    state128_seed(&bs->rng.state, seed, len);
    bs->bits = bs->n_bits = 0;
}

inline void randbs_seed64(struct randbs *bs, uint64_t seed)
{
    state128_seed64(&bs->rng.state, seed);
    bs->bits = bs->n_bits = 0;
}

extern uint64_t randbs_bits(struct randbs *bs, unsigned want_bits);
extern unsigned randbs_zeroes(struct randbs *bs, unsigned limit);

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

/* XXX wrandx functions for struct wrng */

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
#endif
