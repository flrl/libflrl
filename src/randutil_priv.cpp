extern "C" {
#include "flrl/randutil.h"
}

#include <bit>
#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>

template<typename BS, typename T>
void randiv(BS *bs, T *out, std::size_t count, T min, T max)
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
                v = randbs_bits(bs, want_bits);
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
constexpr unsigned extract_exp(TF f)
{
    return (std::bit_cast<TI>(f) >> mantissa_bits<TF>) & exp_mask<TF>;
}

/* based on https://allendowney.com/research/rand/downey07randfloat.pdf */
template<typename BS, typename TF, typename TI>
void randfv(BS *bs, TF *out, std::size_t count, double min, double max)
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
        exponent = high_exp - randbs_zeroes(bs, high_exp - low_exp);

        /* choose a random mantissa */
        mantissa = randbs_bits(bs, mantissa_bits<TF>);

        /* if the mantissa is zero, half the time we should move to the next
         * exponent range */
        if (mantissa == 0 && randbs_bits(bs, 1))
            exponent ++;

        /* combine the exponent and the mantissa */
        val = std::bit_cast<TF, TI>((exponent << mantissa_bits<TF>) | mantissa);

        out[i] = std::fma(range, val, min);
    }
}

template<typename BS, typename TF, typename TI>
void gaussv(BS *bs,
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
TF gauss(BS *bs, double mean, double stddev)
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

} /* extern "C" */
