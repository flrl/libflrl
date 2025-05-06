#include "flrl/perf.h"

#include "flrl/fputil.h"
#include "flrl/statsutil.h"

#include <math.h>

#ifdef _WIN32
static double inv_freq = NAN;
const int64_t min_ticks = 5;
#endif

extern inline void perf_start(struct perf *perf);
extern inline void perf_end(struct perf *perf);

struct perf *perf_new(const char *name, size_t max_samples)
{
    struct perf *perf;

#ifdef _WIN32
    if (isnan(inv_freq)) {
        LARGE_INTEGER tmp;
        QueryPerformanceFrequency(&tmp);
        inv_freq = 1.0 / tmp.QuadPart;
    }
#endif

    perf = calloc(1, sizeof(*perf) + max_samples * sizeof(perf->samples[0]));
    perf->name = strdup(name);
    perf->alloc = max_samples;

    return perf;
}

void perf_free(struct perf *perf)
{
    free(perf->name);
    free(perf);
}

static inline double get_elapsed(const struct perf *perf)
{
    double elapsed;

#ifdef _WIN32
    elapsed = inv_freq * perf->accum.QuadPart / perf->n_accum;
#endif

    return elapsed;
}

static inline void accumulate(struct perf *perf, perf_raw_time ended)
{
    perf_raw_time accum = perf->accum;

#ifdef _WIN32
    accum.QuadPart += ended.QuadPart - perf->started.QuadPart;
#endif

    perf->accum = accum;
    perf->n_accum ++;
}

static inline void finish_sample(struct perf *perf)
{
    size_t next = perf->next;
    perf->samples[next] = get_elapsed(perf);
    perf->accum.QuadPart = 0;
    perf->n_accum = 0;

    next = (next + 1) % perf->alloc;
    perf->next = next;
    if (perf->count < perf->alloc) perf->count ++;
}

void perf_add_sample(struct perf *perf, perf_raw_time ended)
{
    accumulate(perf, ended);

    if (perf->accum.QuadPart >= min_ticks) {
        finish_sample(perf);
    }
}

static const char *format_seconds(double seconds)
{
    static char buf[32] = { 0 };
    const char *const suffix[] = { "ns", "us", "ms", "s", "m", "h", "d" };
    const int colour[] =         {   92,   32,   93,  33,  37,  31,  91 };
    const double divisor[] = { 1000.0, 1000.0, 1000.0, 60.0, 60.0, 24.0 };
    const unsigned n_divisors = sizeof(divisor) / sizeof(divisor[0]);
    const char *fmt;
    unsigned d = 0;

    seconds *= 1000000000.0;

    while (d < n_divisors && seconds >= 1.5 * divisor[d]) {
        seconds = seconds / divisor[d++];
    }

    if (seconds >= 10000.0)
        fmt = "\e[%dm%.0e %s\e[0m";
    else if (seconds >= 1000.0)
        fmt = "\e[%dm%5.0f %s\e[0m";
    else if (seconds < 1.0)
        fmt = "\e[%dm%5.3f %s\e[0m";
    else
        fmt = "\e[%dm%#5.4g %s\e[0m";

    snprintf(buf, sizeof(buf), fmt, colour[d], seconds, suffix[d]);
    return buf;
}

void perf_report(struct perf *perf, FILE *out)
{
    double min, max, mean, variance;
    double median, total;

    if (perf->n_accum) {
        finish_sample(perf);
    }

    total = kbn_sumf64v(perf->samples, perf->count);
    statsf64v(perf->samples, perf->count,
              &min, NULL, &max, NULL, &mean, &variance);
    median = medianf64v(perf->samples, perf->count);

    fprintf(out, "%s (%zu samples)\n", perf->name, perf->count);
    fputs("  total: ", out);
    fputs(format_seconds(total), out);
    fputs("\n    min: ", out);
    fputs(format_seconds(min), out);
    fputs("\n    max: ", out);
    fputs(format_seconds(max), out);
    fputs("\n   mean: ", out);
    fputs(format_seconds(mean), out);
    fputs("\n median: ", out);
    fputs(format_seconds(median), out);
    fputs("\n stddev: ", out);
    fputs(format_seconds(sqrt(variance)), out);
    fputs("\n", out);
}
