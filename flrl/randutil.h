#ifndef LIBFLRL_RANDUTIL_H
#define LIBFLRL_RANDUTIL_H

#include <stdint.h>

struct rng {
    uint32_t (*func)(void *);
    void *state;
};

struct wrng {
    uint64_t (*func)(void *);
    void *state;
};

#define RNG_INIT_XOSHIRO128_PLUS (struct rng){      \
    &xoshiro128plus_next,                           \
    &(struct xoshiro128plus_state){{0}},            \
}

#define RNG_INIT_XOSHIRO128_PLUSPLUS (struct rng){  \
    &xoshiro128plusplus_next,                       \
    &(struct xoshiro128plusplus_state){{0}},        \
}

#define RNG_INIT_XOSHIRO128_STAR (struct rng){      \
    &xoshiro128star_next,                           \
    &(struct xoshiro128star_state){{0}},            \
}

#define WRNG_INIT_XOSHIRO256_PLUS (struct rng){     \
    &xoshiro256plus_next,                           \
    &(struct xoshiro256plus_state){{0}},            \
}

#define WRNG_INIT_XOSHIRO256_PLUSPLUS (struct rng){ \
    &xoshiro256plusplus_next,                       \
    &(struct xoshiro256plusplus_state){{0}},        \
}

#define WRNG_INIT_XOSHIRO256_STAR (struct rng){     \
    &xoshiro256star_next,                           \
    &(struct xoshiro256star_state){{0}},            \
}

extern void randi32v(const struct rng *rng,
                     int32_t *out,
                     size_t count,
                     int32_t min,
                     int32_t max);
extern void randi64v(const struct rng *rng,
                     int64_t *out,
                     size_t count,
                     int64_t min,
                     int64_t max);
extern void randu32v(const struct rng *rng,
                     uint32_t *out,
                     size_t count,
                     uint32_t min,
                     uint32_t max);
extern void randu64v(const struct rng *rng,
                     uint64_t *out,
                     size_t count,
                     uint64_t min,
                     uint64_t max);
extern void randf32v(const struct rng *rng,
                     float *out,
                     size_t count,
                     double min,
                     double max);
extern void randf64v(const struct rng *rng,
                     double *out,
                     size_t count,
                     double min,
                     double max);

inline int32_t randi32(const struct rng *rng, int32_t min, int32_t max)
{
    int32_t v;
    randi32v(rng, &v, 1, min, max);
    return v;
}

inline int64_t randi64(const struct rng *rng, int64_t min, int64_t max)
{
    int64_t v;
    randi64v(rng, &v, 1, min, max);
    return v;
}

inline uint32_t randu32(const struct rng *rng, uint32_t min, uint32_t max)
{
    uint32_t v;
    randu32v(rng, &v, 1, min, max);
    return v;
}

inline uint64_t randu64(const struct rng *rng, uint64_t min, uint64_t max)
{
    uint64_t v;
    randu64v(rng, &v, 1, min, max);
    return v;
}

inline float randf32(const struct rng *rng, double min, double max)
{
    float v;
    randf32v(rng, &v, 1, min, max);
    return v;
}

inline double randf64(const struct rng *rng, double min, double max)
{
    double v;
    randf64v(rng, &v, 1, min, max);
    return v;
}

extern void gaussf32v(const struct rng *rng,
                      float *out,
                      size_t count,
                      double mean,
                      double stddev);

extern float gaussf32(const struct rng *rng, double mean, double stddev);

/* XXX wrandx functions for struct wrng */

/* XXX legacy */
extern uint32_t rand32_inrange(const struct rng *r, uint32_t min, uint32_t max);

extern uint64_t rand64_inrange(const struct wrng *r, uint64_t min, uint64_t max);

extern float rand32f_uniform(const struct rng *r);

extern double rand64f_uniform(const struct wrng *r);

inline int rand32_coin(const struct rng *r, float p_heads)
{
    return (rand32f_uniform(r) < p_heads);
}

inline int rand64_coin(const struct wrng *r, double p_heads)
{
    return (rand64f_uniform(r) < p_heads);
}

struct weight {
    uint16_t weight;
    uint16_t cumulative;
};

/* n weights, build temp cdf, return index */
extern unsigned sample32(const struct rng *r,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample32v(const struct rng *r,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample32p(const struct rng *r,
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

/* for "standard normal distribution", specify mean=0 and stdev=1 */
extern float rand32f_gaussian(const struct rng *rng, float mean, float stdev);

extern double rand64f_gaussian(const struct wrng *r, double mean, double stdev);

#endif