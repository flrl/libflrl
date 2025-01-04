extern "C" {
#include "flrl/randutil.h"
}

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
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
    if (want_bits > RANDBS_MAX_BITS) abort();

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

template<typename TF, typename TI>
    requires (sizeof(TF) == sizeof(TI))
static constexpr unsigned extract_exp(TF f)
{
    return (std::bit_cast<TI>(f) >> mantissa_bits<TF>) & exp_mask<TF>;
}

/* based on https://allendowney.com/research/rand/downey07randfloat.pdf */
template<typename BS, typename TF, typename TI>
    requires (sizeof(TF) == sizeof(TI))
static void randfv(BS *bs, TF *out, std::size_t count, double min, double max)
{
    std::size_t i;

    if (min > max) min = max;
    const double range = max - min;
    if (range > std::numeric_limits<TF>::max()) abort();

    /* we'll generate values in [0, 1], then scale and translate to [min,max]
     * we start with one less than the highest exponent because we may need
     * to add one later
     */
    constexpr unsigned low_exp = extract_exp<TF, TI>(0.0);
    constexpr unsigned high_exp = extract_exp<TF, TI>(1.0) - 1;
    static_assert(high_exp > low_exp);

    for (i = 0; i < count; i++) {
        TI mantissa, exponent;
        TF val;

        /* choose random bits and decrement exponent until a 1 appears */
        exponent = high_exp - bs_zeroes(bs, high_exp - low_exp);

        /* choose a random mantissa */
        mantissa = bs_bits(bs, mantissa_bits<TF>);

        /* if the mantissa is zero, half the time we should move to the next
         * exponent range */
        if (mantissa == 0 && bs_bits(bs, 1))
            exponent ++;

        /* combine the exponent and the mantissa */
        val = std::bit_cast<TF, TI>((exponent << mantissa_bits<TF>) | mantissa);

        out[i] = std::fma(range, val, min);
    }
}

template<typename BS, typename TF, typename TI>
    requires (sizeof(TF) == sizeof(TI))
static void gaussv(BS *bs,
                   TF *out,
                   std::size_t count,
                   double mean,
                   double stddev)
{
    std::size_t i;

    for (i = 0; i < count; i += 2) {
        double v[2], s, t;

        do {
            TF u[2];

            randfv<BS, TF, TI>(bs, u, 2, 0.0, 1.0);

            v[0] = std::fma(2.0, u[0], -1.0);
            v[1] = std::fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = std::sqrt(-2.0 * std::log(s) / s);

        out[i] = std::fma(stddev, v[0] * t, mean);
        if (i + 1 < count)
            out[i + 1] = std::fma(stddev, v[1] * t, mean);
    }
}

template<typename BS, typename TF, typename TI>
    requires (sizeof(TF) == sizeof(TI))
static TF gauss(BS *bs, double mean, double stddev)
{
    static double v[2], t;
    static int phase = 0;
    double x;

    if (0 == phase) {
        double s;

        do {
            TF u[2];

            randfv<BS, TF, TI>(bs, u, 2, 0.0, 1.0);

            v[0] = std::fma(2.0, u[0], -1.0);
            v[1] = std::fma(2.0, u[1], -1.0);
            s = v[0] * v[0] + v[1] * v[1];
        } while (s >= 1.0 || s == 0.0);

        t = std::sqrt(-2.0 * std::log(s) / s);
        x = v[0] * t;
    }
    else {
        x = v[1] * t;
    }

    phase = 1 - phase;

    return std::fma(stddev, x, mean);
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

void randi32v(struct randbs *bs,
              int32_t *out,
              size_t count,
              int32_t min,
              int32_t max)
{
    randiv<struct randbs, int32_t>(bs, out, count, min, max);
}

void randu32v(struct randbs *bs,
              uint32_t *out,
              size_t count,
              uint32_t min,
              uint32_t max)
{
    randiv<struct randbs, uint32_t>(bs, out, count, min, max);
}

void randf32v(struct randbs *bs,
              float *out,
              size_t count,
              double min,
              double max)
{
    randfv<struct randbs, float, uint32_t>(bs, out, count, min, max);
}

void randi64v(struct randbs *bs,
              int64_t *out,
              size_t count,
              int64_t min,
              int64_t max)
{
    randiv<struct randbs, int64_t>(bs, out, count, min, max);
}

void randu64v(struct randbs *bs,
              uint64_t *out,
              size_t count,
              uint64_t min,
              uint64_t max)
{
    randiv<struct randbs, uint64_t>(bs, out, count, min, max);
}

void randf64v(struct randbs *bs,
              double *out,
              size_t count,
              double min,
              double max)
{
    randfv<struct randbs, double, uint64_t>(bs, out, count, min, max);
}

void gaussf32v(struct randbs *bs,
               float *out,
               size_t count,
               double mean,
               double stddev)
{
    gaussv<struct randbs, float, uint32_t>(bs, out, count, mean, stddev);
}

float gaussf32(struct randbs *bs, double mean, double stddev)
{
    return gauss<struct randbs, float, uint32_t>(bs, mean, stddev);
}

void gaussf64v(struct randbs *bs,
               double *out,
               size_t count,
               double mean,
               double stddev)
{
    gaussv<struct randbs, double, uint64_t>(bs, out, count, mean, stddev);
}

double gaussf64(struct randbs *bs, double mean, double stddev)
{
    return gauss<struct randbs, double, uint64_t>(bs, mean, stddev);
}

#if 0
void wrandi32v(struct wrandbs *bs,
               int32_t *out,
               size_t count,
               int32_t min,
               int32_t max)
{
    randiv<struct wrandbs, int32_t>(bs, out, count, min, max);
}

void wrandf32v(struct wrandbs *bs,
               float *out,
               size_t count,
               double min,
               double max)
{
    randfv<struct wrandbs, float, uint32_t>(bs, out, count, min, max);
}
#endif

} /* extern "C" */
