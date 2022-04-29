#include <assert.h>
#include <stdint.h>

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
