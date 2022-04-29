#ifndef LIBFLRL_RANDUTIL_H
#define LIBFLRL_RANDUTIL_H

extern uint32_t rand32_inrange(uint32_t (*randfunc)(void *), void *randstate,
                               uint32_t min, uint32_t max);

extern uint64_t rand64_inrange(uint64_t (*randfunc)(void *), void *randstate,
                               uint64_t min, uint64_t max);
#endif
