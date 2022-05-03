#ifndef LIBFLRL_RANDUTIL_H
#define LIBFLRL_RANDUTIL_H

extern uint32_t rand32_inrange(uint32_t (*randfunc)(void *), void *randstate,
                               uint32_t min, uint32_t max);

extern uint64_t rand64_inrange(uint64_t (*randfunc)(void *), void *randstate,
                               uint64_t min, uint64_t max);

struct weight {
    uint16_t weight;
    uint16_t cumulative;
};

/* n weights, build temp cdf, return index */
extern unsigned sample32(uint32_t (*randfunc)(void *), void *randstate,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample32v(uint32_t (*randfunc)(void *), void *randstate,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample32p(uint32_t (*randfunc)(void *), void *randstate,
                          void *data, size_t rows, size_t rowsize,
                          size_t weight_offset);

/* n weights, build temp cdf, return index */
extern unsigned sample64(uint64_t (*randfunc)(void *), void *randstate,
                         const unsigned weights[], size_t n_weights);

/* n weight|value pairs, build temp cdf, return value */
extern unsigned sample64v(uint64_t (*randfunc)(void *), void *randstate,
                          size_t n_pairs, ...);

/* n elems of size z with struct weight at offset t, save cdf, return index */
extern unsigned sample64p(uint64_t (*randfunc)(void *), void *randstate,
                          void *data, size_t rows, size_t rowsize,
                          size_t weight_offset);

#endif
