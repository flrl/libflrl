#include <string.h>

#include "splitmix64.h"
#include "xoshiro.h"

void xoshiro_seed64(void *statep, size_t size, uint64_t seed)
{
    const size_t n = (size + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    uint64_t buf[n];
    struct splitmix64_state sm64;
    size_t i;

    sm64.x = seed;
    for (i = 0; i < n; i++) {
        buf[i] = splitmix64_next(&sm64);
    }

    memcpy(statep, buf, size);
}
