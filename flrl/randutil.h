#ifndef LIBFLRL_RANDUTIL_H
#define LIBFLRL_RANDUTIL_H

struct rand32 {
    uint32_t (*func)(void *);
    void *state;
};

struct rand64 {
    uint64_t (*func)(void *);
    void *state;
};

extern uint32_t rand32_inrange(const struct rand32 *r, uint32_t min, uint32_t max);

extern uint64_t rand64_inrange(const struct rand64 *r, uint64_t min, uint64_t max);

extern float rand32f_uniform(const struct rand32 *r);

extern double rand64f_uniform(const struct rand64 *r);

inline int rand32_coin(const struct rand32 *r, float p_heads)
{
    return (rand32f_uniform(r) < p_heads);
}

inline int rand64_coin(const struct rand64 *r, double p_heads)
{
    return (rand64f_uniform(r) < p_heads);
}

struct weight {
    uint16_t weight;
    uint16_t cumulative;
};

/* n weights, build temp cdf, return index */
extern unsigned sample32(const struct rand32 *r,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample32v(const struct rand32 *r,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample32p(const struct rand32 *r,
                          void *data, size_t rows, size_t rowsize,
                          size_t weight_offset);

/* n weights, build temp cdf, return index */
extern unsigned sample64(const struct rand64 *r,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample64v(const struct rand64 *r,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample64p(const struct rand64 *r,
                          void *data, size_t rows, size_t rowsize,
                          size_t weight_offset);

/* for "standard normal distribution", specify mean=0 and stdev=1 */
extern float rand32f_gaussian(const struct rand32 *r, float mean, float stdev);

extern double rand64f_gaussian(const struct rand64 *r, double mean, double stdev);

#endif
