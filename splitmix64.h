#ifndef LIBFLRL_SPLITMIX64_H
#define LIBFLRL_SPLITMIX64_H

/* This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush, and it can be useful if
   for some reason you absolutely want 64 bits of state. */
struct splitmix64_state {
    uint64_t x; /* The state can be seeded with any value. */
};
extern uint64_t splitmix64_next(struct splitmix64_state *state);

#endif
