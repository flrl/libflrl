extern "C" {
#include "flrl/statsutil.h"

#include "flrl/fputil.h"
#include "flrl/hashmap.h"

extern const double statsutil_nan;

extern void *statsutil_malloc(size_t size);
extern void *statsutil_calloc(size_t nelem, size_t elsize);
extern char *statsutil_strdup(const char *s);
extern void statsutil_free(void *ptr);

extern double statsutil_ceil(double x);
extern double statsutil_floor(double x);
extern double statsutil_round(double x);

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
#include <cassert>
#include <cstring>
#include <limits>
#include <type_traits>

template<typename T>
static double mean(const T *values, std::size_t n_values)
{
    double mean = 0, c = 0, scale = 1.0 / n_values;
    std::size_t i;

    if (!n_values) return statsutil_nan;

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

    if (!n_values) return statsutil_nan;

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
static int summary5(const T *values, std::size_t n_values, double quartiles[5])
{
    T *copy = NULL;
    double q0, q1, q2, q3, q4;

    q0 = q1 = q2 = q3 = q4 = statsutil_nan;

    switch (n_values) {
        case 2:
            q0 = std::min(values[0], values[1]);
            q2 = 0.5 * (values[0] + values[1]);
            q4 = std::max(values[0], values[1]);
            goto done;
        case 1:
            q0 = q4 = values[0];
            goto done;
        case 0:
            goto done;
    }

    copy = (T*) statsutil_malloc(n_values * sizeof(values[0]));
    if (!copy) return -1;

    std::copy(values, values + n_values, copy);
    std::sort(copy, copy + n_values);

    q0 = copy[0];
    q4 = copy[n_values - 1];

    if (n_values == 3) {
        q2 = copy[1];
    }
    else {
        /* https://en.wikipedia.org/wiki/Quartile#Method_4 */
        std::size_t k1, k2, k3;
        double a1, a2, a3;

        k1 = a1 = 0.25 * (n_values + 1);
        k2 = a2 = 0.5 * (n_values + 1);
        k3 = a3 = 0.75 * (n_values + 1);

        a1 -= k1;
        a2 -= k2;
        a3 -= k3;

        assert(k1 > 0);
        k1--;
        k2--;
        k3--;

        q1 = copy[k1] + a1 * (copy[k1 + 1] - copy[k1]);
        q2 = copy[k2] + a2 * (copy[k2 + 1] - copy[k2]);
        q3 = copy[k3] + a3 * (copy[k3 + 1] - copy[k3]);
    }

    statsutil_free(copy);

 done:
    quartiles[0] = q0;
    quartiles[1] = q1;
    quartiles[2] = q2;
    quartiles[3] = q3;
    quartiles[4] = q4;

    return 0;
}

template<typename T>
static T mode(const T *values, std::size_t n_values, std::size_t *pfrequency)
{
    HashMap counts;
    struct fmv_ctx fmv_ctx = { NULL, NULL };
    size_t i;
    T mode;

    static_assert(std::is_integral<T>::value, "Integral required");

    if (!n_values) {
        if (pfrequency) *pfrequency = 0;
        return 0;
    }

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
    double variance = 0, c = 0, scale;
    std::size_t i;

    if (!n_values) return statsutil_nan;

    scale = 1.0 / (n_values - 1);

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
    double mean, scale, variance, c;
    T min, max;
    std::size_t i, min_freq, max_freq;

    min = std::numeric_limits<T>::max();
    max = std::numeric_limits<T>::lowest();

    /* min, max, mean */
    min_freq = max_freq = mean = c = 0;
    scale = 1.0 / n_values;
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
    scale = 1.0 / (n_values - 1);
    for (i = 0; i < n_values; i++) {
        double diff;

        diff = values[i] - mean;
        kbn_sumf64_r(&variance, &c, diff * diff);
    }
    *pvariance = scale * (variance + c);
}

template<typename T>
requires std::is_integral_v<T> || std::is_floating_point_v<T>
static T *invent_thresholds(const T *values, std::size_t n_values,
                            std::size_t *pn_thresholds)
{
    T lb, ub, range, step;
    T *thresholds;
    std::size_t i, n_thresholds;

    lb = std::numeric_limits<T>::max();
    ub = std::numeric_limits<T>::lowest();
    for (i = 0; i < n_values; i++) {
        lb = std::min(lb, values[i]);
        ub = std::max(ub, values[i]);
    }

    if (std::is_floating_point_v<T>) {
        ub = niceceil(ub);
        range = niceceil(ub - lb);
        lb = ub - range;
    }
    else {
        if (ub < std::numeric_limits<T>::max())
            ub ++;
        range = ub - lb;
    }

    n_thresholds = *pn_thresholds;
    if (n_thresholds == 0) {
        double t = range;

        while (t >= 18.0)
            t *= 1.0 / 3.0;

        while (t < 6.0)
            t *= 3.0;

        n_thresholds = statsutil_ceil(t);
    }

    if (std::is_floating_point_v<T>) {
        step = niceceil(range / n_thresholds);
    }
    else {
        n_thresholds = std::min(n_thresholds, (size_t) range);
        step = statsutil_ceil(1.0 * range / n_thresholds);
    }

    thresholds = (T*) statsutil_malloc(n_thresholds * sizeof(thresholds[0]));
    if (!thresholds) return NULL;

    if (lb == std::numeric_limits<T>::lowest())
        lb += step;
    for (i = 0; i < n_thresholds; i++) {
        thresholds[i] = i * step + lb;
    }

    *pn_thresholds = n_thresholds;
    return thresholds;
}

template<typename T>
static void histogram_freq(Histogram *hist, const char *title,
                           const T *values, std::size_t n_values,
                           const T *thresholds, std::size_t n_thresholds)
{
    std::size_t min_freq_raw = std::numeric_limits<std::size_t>::max();
    std::size_t max_freq_raw = 0;
    std::size_t i, t;
    double fpp;
    T *freeme = NULL;

    if (!thresholds) {
        thresholds = freeme = invent_thresholds(values, n_values,
                                                &n_thresholds);
    }

    memset(hist, 0, sizeof(*hist));
    hist->title = statsutil_strdup(title);
    hist->n_buckets = n_thresholds + 1;
    hist->buckets = (hist_bucket *) statsutil_calloc(hist->n_buckets,
                                                     sizeof(hist->buckets[0]));

    /* count raw frequencies */
    for (i = 0; i < n_values; i++) {
        for (t = 0; t < n_thresholds; t++) {
            if (values[i] < thresholds[t]) {
                hist->buckets[t].freq_raw ++;
                break;
            }
        }
        if (t == n_thresholds)
            hist->buckets[t].freq_raw ++;
    }

    /* compute percent frequencies, min/max raw frequency, and labels */
    for (i = 0; i < hist->n_buckets; i++) {
        struct hist_bucket *bucket = &hist->buckets[i];

        bucket->freq_pc = 100.0 * bucket->freq_raw / n_values;
        min_freq_raw = std::min(min_freq_raw, bucket->freq_raw);
        max_freq_raw = std::max(max_freq_raw, bucket->freq_raw);

        bucket->skip_if_zero = false;

        if (i == 0) {
            if (std::numeric_limits<T>::is_signed)
                strcpy(bucket->lb_label, "-inf");
            else
                strcpy(bucket->lb_label, "0");
            bucket->skip_if_zero = true;
        }
        else {
            snprintf(bucket->lb_label, sizeof(bucket->lb_label),
                     "%g", (double) thresholds[i - 1]);
        }

        if (i == n_thresholds) {
            strcpy(bucket->ub_label, "+inf");
            bucket->skip_if_zero = true;
        }
        else {
            snprintf(bucket->ub_label, sizeof(bucket->ub_label),
                     "<%g", (double) thresholds[i]);
        }
    }

    /* compute grid lines and pips */
    fpp = niceceil(max_freq_raw / 60.0);
    for (i = 0; i < 6; i++) {
        hist->grid[i] = (i + 1) * fpp * 10.0;
    }
    for (i = 0; i < hist->n_buckets; i++) {
        struct hist_bucket *bucket = &hist->buckets[i];
        bucket->pips = statsutil_round(bucket->freq_raw / fpp);
    }
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

int summary5i8v(const int8_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5u8v(const uint8_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5i16v(const int16_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5u16v(const uint16_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5i32v(const int32_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5u32v(const uint32_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5i64v(const int64_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5u64v(const uint64_t *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5f32v(const float *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

int summary5f64v(const double *values, size_t n_values, double quartiles[5])
{
    return summary5(values, n_values, quartiles);
}

void histogram_freqi8v(Histogram *hist, const char *title,
                       const int8_t *values, size_t n_values,
                       const int8_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_frequ8v(Histogram *hist, const char *title,
                       const uint8_t *values, size_t n_values,
                       const uint8_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_freqi16v(Histogram *hist, const char *title,
                        const int16_t *values, size_t n_values,
                        const int16_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_frequ16v(Histogram *hist, const char *title,
                        const uint16_t *values, size_t n_values,
                        const uint16_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_freqi32v(Histogram *hist, const char *title,
                        const int32_t *values, size_t n_values,
                        const int32_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_frequ32v(Histogram *hist, const char *title,
                        const uint32_t *values, size_t n_values,
                        const uint32_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_freqi64v(Histogram *hist, const char *title,
                        const int64_t *values, size_t n_values,
                        const int64_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_frequ64v(Histogram *hist, const char *title,
                        const uint64_t *values, size_t n_values,
                        const uint64_t *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_freqf32v(Histogram *hist, const char *title,
                        const float *values, size_t n_values,
                        const float *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

void histogram_freqf64v(Histogram *hist, const char *title,
                        const double *values, size_t n_values,
                        const double *thresholds, size_t n_thresholds)
{
    histogram_freq(hist, title, values, n_values, thresholds, n_thresholds);
}

} /* extern "C" */
