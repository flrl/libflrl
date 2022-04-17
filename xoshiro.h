#ifndef LIBFLRL_XOSHIRO_H
#define LIBFLRL_XOSHIRO_H

/* This is xoshiro128+ 1.0, our best and fastest 32-bit generator for 32-bit
   floating-point numbers. We suggest to use its upper bits for
   floating-point generation, as it is slightly faster than xoshiro128**.
   It passes all tests we are aware of except for
   linearity tests, as the lowest four bits have low linear complexity, so
   if low linear complexity is not considered an issue (as it is usually
   the case) it can be used to generate 32-bit outputs, too.

   We suggest to use a sign test to extract a random Boolean value, and
   right shifts to extract subsets of bits.

   The state must be seeded so that it is not everywhere zero. */
struct xoshiro128plus_state {
    uint32_t s[4];
};
extern uint32_t xoshiro128plus_next(struct xoshiro128plus_state *state);
extern void xoshiro128plus_long_jump(struct xoshiro128plus_state *state);
extern void xoshiro128plus_jump(struct xoshiro128plus_state *state);


#endif
