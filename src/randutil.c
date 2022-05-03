#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "randutil.h"

uint32_t rand32_inrange(uint32_t (*randfunc)(void *), void *randstate,
                        uint32_t min, uint32_t max)
{
    uint32_t range;
    uint32_t value;
    int needloop;

    assert(max > min);
    range = 1 + max - min; /* inclusive */
    needloop = (0 != UINT32_MAX % range);

    do {
        value = randfunc(randstate);
    } while (needloop && value > UINT32_MAX - range);

    return min + value % range;
}

uint64_t rand64_inrange(uint64_t (*randfunc)(void *), void *randstate,
                        uint64_t min, uint64_t max)
{
    uint64_t range;
    uint64_t value;
    int needloop;

    assert(max > min);
    range = 1 + max - min; /* inclusive */
    needloop = (0 != UINT64_MAX % range);

    do {
        value = randfunc(randstate);
    } while (needloop && value > UINT64_MAX - range);

    return min + value % range;
}

unsigned sample32(uint32_t (*randfunc)(void *), void *randstate,
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

    rand = rand32_inrange(randfunc, randstate, 0, sum - 1);
    for (i = 0; i < n_weights && rand >= cdf[i]; i++)
        ;

    free(cdf);
    return i;
}

unsigned sample32v(uint32_t (*randfunc)(void *), void *randstate,
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

    rand = rand32_inrange(randfunc, randstate, 0, sum - 1);
    for (i = 0; i < n_pairs && rand >= cdf[i]; i++)
        ;

    ret = values[i];
    free(values);
    free(cdf);

    return ret;
}

/* n elems of size z with struct weight at offset t, save cdf, return index */
unsigned sample32p(uint32_t (*randfunc)(void *), void *randstate,
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

    /* lazy load cdf when first cumulative value is zero */
    wp = (struct weight *)(data + weight_offset);
    if (wp->cumulative == 0) {
        prev = NULL;
        for (p = data; p < (data + rows * rowsize); p += rowsize) {
            wp = (struct weight *)(p + weight_offset);
            sum += wp->weight;
            assert(prev == NULL || sum >= prev->cumulative); /* overflow */
            wp->cumulative = sum;
        }
    }
    assert(sum > 0);
    if (sum == 0) return 0;

    rand = rand32_inrange(randfunc, randstate, 0, sum - 1);

    prev = NULL;
    i = 0;
    do {
        wp = (struct weight *)((data + i * rowsize) + weight_offset);

        if (rand < wp->cumulative
            && (prev == NULL || rand >= prev->cumulative))
        {
            break;
        }

        i++;
        prev = wp;
    } while (i < rows);

    return i;
}

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
