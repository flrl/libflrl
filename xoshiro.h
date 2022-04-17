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
extern void xoshiro128plus_jump(struct xoshiro128plus_state *state);
extern void xoshiro128plus_long_jump(struct xoshiro128plus_state *state);


/* This is xoshiro128++ 1.0, one of our 32-bit all-purpose, rock-solid
   generators. It has excellent speed, a state size (128 bits) that is
   large enough for mild parallelism, and it passes all tests we are aware
   of.

   For generating just single-precision (i.e., 32-bit) floating-point
   numbers, xoshiro128+ is even faster.

   The state must be seeded so that it is not everywhere zero. */
struct xoshiro128plusplus_state {
    uint32_t s[4];
};
extern uint32_t xoshiro128plusplus_next(struct xoshiro128plusplus_state *state);
extern void xoshiro128plusplus_jump(struct xoshiro128plusplus_state *state);
extern void xoshiro128plusplus_long_jump(struct xoshiro128plusplus_state *state);


/* This is xoshiro128** 1.1, one of our 32-bit all-purpose, rock-solid
   generators. It has excellent speed, a state size (128 bits) that is
   large enough for mild parallelism, and it passes all tests we are aware
   of.

   Note that version 1.0 had mistakenly s[0] instead of s[1] as state
   word passed to the scrambler.

   For generating just single-precision (i.e., 32-bit) floating-point
   numbers, xoshiro128+ is even faster.

   The state must be seeded so that it is not everywhere zero. */
struct xoshiro128starstar_state {
    uint32_t s[4];
};
extern uint32_t xoshiro128starstar_next(struct xoshiro128starstar_state *state);
extern void xoshiro128starstar_jump(struct xoshiro128starstar_state *state);
extern void xoshiro128starstar_long_jump(struct xoshiro128starstar_state *state);


/* This is xoshiro256+ 1.0, our best and fastest generator for floating-point
   numbers. We suggest to use its upper bits for floating-point
   generation, as it is slightly faster than xoshiro256++/xoshiro256**. It
   passes all tests we are aware of except for the lowest three bits,
   which might fail linearity tests (and just those), so if low linear
   complexity is not considered an issue (as it is usually the case) it
   can be used to generate 64-bit outputs, too.

   We suggest to use a sign test to extract a random Boolean value, and
   right shifts to extract subsets of bits.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */
struct xoshiro256plus_state {
    uint64_t s[4];
};
extern uint64_t xoshiro256plus_next(struct xoshiro256plus_state *state);
extern void xoshiro256plus_jump(struct xoshiro256plus_state *state);
extern void xoshiro256plus_long_jump(struct xoshiro256plus_state *state);


/* This is xoshiro256++ 1.0, one of our all-purpose, rock-solid generators.
   It has excellent (sub-ns) speed, a state (256 bits) that is large
   enough for any parallel application, and it passes all tests we are
   aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

struct xoshiro256plusplus_state {
    uint64_t s[4];
};
extern uint64_t xoshiro256plusplus_next(struct xoshiro256plusplus_state *state);
extern void xoshiro256plusplus_jump(struct xoshiro256plusplus_state *state);
extern void xoshiro256plusplus_long_jump(struct xoshiro256plusplus_state *state);

#endif
