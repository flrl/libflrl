extern "C" {
#include "flrl/randutil.h"

#include "flrl/xassert.h"
}

#include <algorithm>
#include <bit>
#include <climits>
#include <limits>
#include <type_traits>

static inline uint64_t mask_bits(unsigned bits)
{
    return bits < 64
           ? (UINT64_C(1) << bits) - 1
           : UINT64_C(0) - 1;
}

template<typename BS>
constexpr unsigned rng_bits;

template<>
constexpr unsigned rng_bits<struct randbs> = 32;

template<>
constexpr unsigned rng_bits<struct wrandbs> = 64;

template<typename BS>
static uint64_t bs_bits(BS *bs, unsigned want_bits)
{
    uint64_t bits, extra = 0;

    if (!want_bits) return 0;
    hard_assert(want_bits <= RANDBS_MAX_BITS);

    while (bs->n_bits < want_bits) {
        uint64_t v;
        unsigned b;

        v = bs->func(&bs->state);
        b = RANDBS_MAX_BITS - bs->n_bits;

        bs->bits |= v << bs->n_bits;
        extra = b < rng_bits<BS> ? v >> b : 0;
        bs->n_bits += rng_bits<BS>;
    }

    bits = bs->bits & mask_bits(want_bits);

    bs->bits = want_bits < RANDBS_MAX_BITS ? bs->bits >> want_bits : 0;
    if (extra)
        bs->bits |= extra << (RANDBS_MAX_BITS - want_bits);
    bs->n_bits -= want_bits;

    return bits;
}

template<typename BS>
static unsigned bs_zeroes(BS *bs, unsigned limit)
{
    unsigned zeroes = 0;
    bool saw_one = false;

    limit = std::clamp(limit, 0U, (unsigned) INT_MAX);

    while (limit && !saw_one) {
        unsigned z, x;

        if (bs->n_bits == 0) {
            bs->bits = bs->func(&bs->state);
            bs->n_bits = rng_bits<BS>;
        }

        /* if the string of zeroes was stopped by a one, need to consume it
         * otherwise next bit is guaranteed to be one.  especially in the
         * case where there were no zeroes, in which case the state of the
         * rng won't advance if we don't consume at least one bit
         */
        x = std::min(limit, bs->n_bits);
        z = std::countr_zero(bs->bits);
        if (z >= x)
            z = x;
        else
            saw_one = true;

        zeroes += z;
        limit -= z;
        bs->bits >>= z + saw_one;
        bs->n_bits -= z + saw_one;
    }

    return zeroes;
}

template<typename BS, typename T>
static void randiv(BS *bs, T *out, std::size_t count, T min, T max)
{
    using UT = std::make_unsigned_t<T>;
    UT range;
    std::size_t i;

    if (min > max) min = max;

    range = max - min;

    if (range == 0) {
        for (i = 0; i < count; i++) {
            out[i] = min;
        }
    }
    else {
        unsigned want_bits = std::bit_width(range);

        /* XXX adjust want_bits for perverse cases? */

        for (i = 0; i < count; i++) {
            uint64_t v;

            do {
                v = bs_bits(bs, want_bits);
            } while (v > range);

            out[i] = min + v;
        }
    }
}

template<typename T>
constexpr unsigned exp_mask;

template<>
constexpr unsigned exp_mask<float> = 0xff;

template<>
constexpr unsigned exp_mask<double> = 0x7ff;

template<typename T>
constexpr unsigned mantissa_bits = std::numeric_limits<T>::digits - 1;

template<typename T>
    requires (std::is_floating_point<T>::value
              && (sizeof(T) == sizeof(uint32_t)
                  || sizeof(T) == sizeof(uint64_t)))
using uintf = typename std::conditional<sizeof(T) == sizeof(uint32_t),
                                        uint32_t,
                                        uint64_t>::type;

template<typename T>
static constexpr unsigned extract_exp(T f)
{
    return (std::bit_cast<uintf<T>>(f) >> mantissa_bits<T>) & exp_mask<T>;
}

/* based on https://allendowney.com/research/rand/downey07randfloat.pdf */
template<typename BS, typename T>
static void randfv(BS *bs, T *out, std::size_t count, double min, double max)
{
    std::size_t i;

    if (min > max) min = max;
    const double range = max - min;
    hard_assert(range <= std::numeric_limits<T>::max());

    /* we'll generate values in [0, 1], then scale and translate to [min,max]
     * we start with one less than the highest exponent because we may need
     * to add one later
     */
    constexpr unsigned low_exp = extract_exp<T>(0.0);
    constexpr unsigned high_exp = extract_exp<T>(1.0) - 1;
    static_assert(high_exp > low_exp);

    for (i = 0; i < count; i++) {
        uintf<T> mantissa, exponent;
        T val;

        /* choose random bits and decrement exponent until a 1 appears */
        exponent = high_exp - bs_zeroes(bs, high_exp - low_exp);

        /* choose a random mantissa */
        mantissa = bs_bits(bs, mantissa_bits<T>);

        /* if the mantissa is zero, half the time we should move to the next
         * exponent range */
        if (mantissa == 0 && bs_bits(bs, 1))
            exponent ++;

        /* combine the exponent and the mantissa */
        val = std::bit_cast<T, uintf<T>>((exponent << mantissa_bits<T>)
                                         | mantissa);

        out[i] = randutil_fma(range, val, min);
    }
}

template<typename BS, typename T>
static void gaussv(BS *bs,
                   T *out,
                   std::size_t count,
                   double mean,
                   double stddev)
{
    std::size_t i;

    for (i = 0; i < count; i += 2) {
        double v[2], s, t;

        do {
            T u[2];

            randfv(bs, u, 2, 0.0, 1.0);

            v[0] = randutil_fma(2.0, u[0], -1.0);
            v[1] = randutil_fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = randutil_sqrt(-2.0 * randutil_log(s) / s);

        out[i] = randutil_fma(stddev, v[0] * t, mean);
        if (i + 1 < count)
            out[i + 1] = randutil_fma(stddev, v[1] * t, mean);
    }
}

template<typename BS, typename T>
static T gauss(BS *bs, double mean, double stddev)
{
    static double v[2], t;
    static int phase = 0;
    double x;

    if (0 == phase) {
        double s;

        do {
            T u[2];

            randfv(bs, u, 2, 0.0, 1.0);

            v[0] = randutil_fma(2.0, u[0], -1.0);
            v[1] = randutil_fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = randutil_sqrt(-2.0 * randutil_log(s) / s);
        x = v[0] * t;
    }
    else {
        x = v[1] * t;
    }

    phase = 1 - phase;

    return randutil_fma(stddev, x, mean);
}

extern "C" {

uint64_t randbs_bits(struct randbs *bs, unsigned want_bits)
{
    return bs_bits(bs, want_bits);
}

unsigned randbs_zeroes(struct randbs *bs, unsigned limit)
{
    return bs_zeroes(bs, limit);
}

void randi8v(struct randbs *bs,
             int8_t *out,
             size_t count,
             int8_t min,
             int8_t max)
{
    randiv(bs, out, count, min, max);
}

void randi16v(struct randbs *bs,
              int16_t *out,
              size_t count,
              int16_t min,
              int16_t max)
{
    randiv(bs, out, count, min, max);
}

void randi32v(struct randbs *bs,
              int32_t *out,
              size_t count,
              int32_t min,
              int32_t max)
{
    randiv(bs, out, count, min, max);
}

void randi64v(struct randbs *bs,
              int64_t *out,
              size_t count,
              int64_t min,
              int64_t max)
{
    randiv(bs, out, count, min, max);
}

void randu8v(struct randbs *bs,
             uint8_t *out,
             size_t count,
             uint8_t min,
             uint8_t max)
{
    randiv(bs, out, count, min, max);
}

void randu16v(struct randbs *bs,
              uint16_t *out,
              size_t count,
              uint16_t min,
              uint16_t max)
{
    randiv(bs, out, count, min, max);
}

void randu32v(struct randbs *bs,
              uint32_t *out,
              size_t count,
              uint32_t min,
              uint32_t max)
{
    randiv(bs, out, count, min, max);
}

void randu64v(struct randbs *bs,
              uint64_t *out,
              size_t count,
              uint64_t min,
              uint64_t max)
{
    randiv(bs, out, count, min, max);
}

void randf32v(struct randbs *bs,
              float *out,
              size_t count,
              double min,
              double max)
{
    randfv(bs, out, count, min, max);
}

void randf64v(struct randbs *bs,
              double *out,
              size_t count,
              double min,
              double max)
{
    randfv(bs, out, count, min, max);
}

void gaussf32v(struct randbs *bs,
               float *out,
               size_t count,
               double mean,
               double stddev)
{
    gaussv(bs, out, count, mean, stddev);
}

float gaussf32(struct randbs *bs, double mean, double stddev)
{
    return gauss<struct randbs, float>(bs, mean, stddev);
}

void gaussf64v(struct randbs *bs,
               double *out,
               size_t count,
               double mean,
               double stddev)
{
    gaussv(bs, out, count, mean, stddev);
}

double gaussf64(struct randbs *bs, double mean, double stddev)
{
    return gauss<struct randbs, double>(bs, mean, stddev);
}

#if 0
void wrandi32v(struct wrandbs *bs,
               int32_t *out,
               size_t count,
               int32_t min,
               int32_t max)
{
    randiv(bs, out, count, min, max);
}

void wrandf32v(struct wrandbs *bs,
               float *out,
               size_t count,
               double min,
               double max)
{
    randfv(bs, out, count, min, max);
}
#endif

} /* extern "C" */
