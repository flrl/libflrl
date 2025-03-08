extern "C" {
#include "flrl/statsutil.h"

#include "flrl/fputil.h"
#include "flrl/hashmap.h"

struct fmv_ctx {
    const void *max_key;
    void *max_value;
};
static int find_max_value(const HashMap *hm,
                          const void *key,
                          size_t key_len,
                          void *value,
                          void *ctx);
}

#include <algorithm>
#include <limits>
#include <type_traits>

template<typename T>
static double mean(const T *values, std::size_t n_values)
{
    double mean = 0, c = 0, scale = 1.0 / n_values;
    std::size_t i;

    for (i = 0; i < n_values; i++) {
        kbn_sumf64_r(&mean, &c, scale * values[i]);
    }

    return mean + c;
}

template<typename T>
static double median(const T *values, std::size_t n_values)
{
    T *copy;
    double median;

    copy = (T*) statsutil_malloc(n_values * sizeof(values[0]));
    if (!copy) return statsutil_nan;

    std::copy(values, values + n_values, copy);
    std::sort(copy, copy + n_values);

    median = copy[n_values / 2];

    if (!(n_values & 1))
        median = 0.5 * (median + copy[n_values / 2 - 1]);

    statsutil_free(copy);
    return median;
}

template<typename T>
static T mode(const T *values, std::size_t n_values, std::size_t *pfrequency)
{
    HashMap counts;
    struct fmv_ctx fmv_ctx = { NULL, NULL };
    size_t i;
    T mode;

    static_assert(std::is_integral<T>::value, "Integral required");

    hashmap_init(&counts, n_values / 10);

    for (i = 0; i < n_values; i++) {
        uintptr_t count;

        /* XXX hashmap_mod */
        hashmap_get(&counts, &values[i], sizeof(values[i]),
                    reinterpret_cast<void **>(&count));
        hashmap_put(&counts, &values[i], sizeof(values[i]),
                    reinterpret_cast<void *>(count + 1), NULL);
    }

    hashmap_foreach(&counts, &find_max_value, &fmv_ctx);

    mode = *static_cast<const T *>(fmv_ctx.max_key);
    if (pfrequency) {
        uintptr_t frequency = reinterpret_cast<uintptr_t>(fmv_ctx.max_value);
        *pfrequency = frequency;
    }

    hashmap_fini(&counts, NULL);
    return mode;
}

template<typename T>
static double variance(const T *values, std::size_t n_values, double mean)
{
    double variance = 0, c = 0, scale = 1.0 / n_values;
    std::size_t i;

    for (i = 0; i < n_values; i++) {
        double diff;

        diff = values[i] - mean;
        kbn_sumf64_r(&variance, &c, diff * diff);
    }

    return scale * (variance + c);
}

template<typename T>
static void stats(const T *values, std::size_t n_values,
                  T *pmin, size_t *pmin_frequency,
                  T *pmax, size_t *pmax_frequency,
                  double *pmean, double *pvariance)
{
    const double scale = 1.0 / n_values;
    double mean, variance, c;
    T min, max;
    std::size_t i, min_freq, max_freq;

    min = std::numeric_limits<T>::max();
    max = std::numeric_limits<T>::lowest();

    /* min, max, mean */
    min_freq = max_freq = mean = c = 0;
    for (i = 0; i < n_values; i++) {
        if (values[i] < min) {
            min = values[i];
            min_freq = 1;
        }
        else if (values[i] == min) {
            min_freq ++;
        }

        if (values[i] > max) {
            max = values[i];
            max_freq = 1;
        }
        else if (values[i] == max) {
            max_freq ++;
        }

        kbn_sumf64_r(&mean, &c, scale * values[i]);
    }
    mean += c;

    if (pmin) *pmin = min;
    if (pmin_frequency) *pmin_frequency = min_freq;
    if (pmax) *pmax = max;
    if (pmax_frequency) *pmax_frequency = max_freq;
    if (pmean) *pmean = mean;

    if (!pvariance) return;

    /* variance */
    variance = c = 0;
    for (i = 0; i < n_values; i++) {
        double diff;

        diff = values[i] - mean;
        kbn_sumf64_r(&variance, &c, diff * diff);
    }
    *pvariance = scale * (variance + c);
}

extern "C" {
static int find_max_value(const HashMap *hm __attribute__((unused)),
                          const void *key,
                          size_t key_len __attribute__((unused)),
                          void *value,
                          void *ctx)
{
    struct fmv_ctx *fmv_ctx = static_cast<struct fmv_ctx *>(ctx);

    if (value > fmv_ctx->max_value) {
        fmv_ctx->max_key = key;
        fmv_ctx->max_value = value;
    }

    return 0;
}

double meani8v(const int8_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double mediani8v(const int8_t *values, size_t n_values)
{
    return median(values, n_values);
}

int8_t modei8v(const int8_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double variancei8v(const int8_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meanu8v(const uint8_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double medianu8v(const uint8_t *values, size_t n_values)
{
    return median(values, n_values);
}

uint8_t modeu8v(const uint8_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double varianceu8v(const uint8_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meani16v(const int16_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double mediani16v(const int16_t *values, size_t n_values)
{
    return median(values, n_values);
}

int16_t modei16v(const int16_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double variancei16v(const int16_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meanu16v(const uint16_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double medianu16v(const uint16_t *values, size_t n_values)
{
    return median(values, n_values);
}

uint16_t modeu16v(const uint16_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double varianceu16v(const uint16_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meani32v(const int32_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double mediani32v(const int32_t *values, size_t n_values)
{
    return median(values, n_values);
}

int32_t modei32v(const int32_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double variancei32v(const int32_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meanu32v(const uint32_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double medianu32v(const uint32_t *values, size_t n_values)
{
    return median(values, n_values);
}

uint32_t modeu32v(const uint32_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double varianceu32v(const uint32_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meani64v(const int64_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double mediani64v(const int64_t *values, size_t n_values)
{
    return median(values, n_values);
}

int64_t modei64v(const int64_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double variancei64v(const int64_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meanu64v(const uint64_t *values, size_t n_values)
{
    return mean(values, n_values);
}

double medianu64v(const uint64_t *values, size_t n_values)
{
    return median(values, n_values);
}

uint64_t modeu64v(const uint64_t *values, size_t n_values, size_t *pfrequency)
{
    return mode(values, n_values, pfrequency);
}

double varianceu64v(const uint64_t *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meanf32v(const float *values, size_t n_values)
{
    return mean(values, n_values);
}

double medianf32v(const float *values, size_t n_values)
{
    return median(values, n_values);
}

double variancef32v(const float *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

double meanf64v(const double *values, size_t n_values)
{
    return mean(values, n_values);
}

double medianf64v(const double *values, size_t n_values)
{
    return median(values, n_values);
}

double variancef64v(const double *values, size_t n_values, double mean)
{
    return variance(values, n_values, mean);
}

void statsi8v(const int8_t *values, size_t n_values,
              int8_t *pmin, size_t *pmin_frequency,
              int8_t *pmax, size_t *pmax_frequency,
              double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsu8v(const uint8_t *values, size_t n_values,
              uint8_t *pmin, size_t *pmin_frequency,
              uint8_t *pmax, size_t *pmax_frequency,
              double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsi16v(const int16_t *values, size_t n_values,
               int16_t *pmin, size_t *pmin_frequency,
               int16_t *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsu16v(const uint16_t *values, size_t n_values,
               uint16_t *pmin, size_t *pmin_frequency,
               uint16_t *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsi32v(const int32_t *values, size_t n_values,
               int32_t *pmin, size_t *pmin_frequency,
               int32_t *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsu32v(const uint32_t *values, size_t n_values,
               uint32_t *pmin, size_t *pmin_frequency,
               uint32_t *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsi64v(const int64_t *values, size_t n_values,
               int64_t *pmin, size_t *pmin_frequency,
               int64_t *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsu64v(const uint64_t *values, size_t n_values,
               uint64_t *pmin, size_t *pmin_frequency,
               uint64_t *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsf32v(const float *values, size_t n_values,
               float *pmin, size_t *pmin_frequency,
               float *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

void statsf64v(const double *values, size_t n_values,
               double *pmin, size_t *pmin_frequency,
               double *pmax, size_t *pmax_frequency,
               double *pmean, double *pvariance)
{
    return stats(values, n_values,
                 pmin, pmin_frequency,
                 pmax, pmax_frequency,
                 pmean, pvariance);
}

} /* extern "C" */
