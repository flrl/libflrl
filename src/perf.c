#include "flrl/perf.h"

#include "flrl/fputil.h"

#include <math.h>

#ifdef _WIN32
static double inv_freq = NAN;
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

static inline double get_elapsed(perf_raw_time start, perf_raw_time end)
{
    double elapsed;
#ifdef _WIN32
    elapsed = inv_freq * (end.QuadPart - start.QuadPart);
#endif

    return elapsed >= 0 ? elapsed : 0.0;
}

void perf_add_sample(struct perf *perf, perf_raw_time ended)
{
    size_t next = perf->next;

    perf->samples[next] = get_elapsed(perf->started, ended);
    next = (next + 1) % perf->alloc;
    perf->next = next;
    if (perf->count < perf->alloc) perf->count ++;
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

static int cmp_double(const void *a, const void *b)
{
    double aa = *(const double *) a;
    double bb = *(const double *) b;

    return (aa > bb) - (aa < bb);
}

void perf_report(struct perf *perf, FILE *out)
{
    const double scale = 1.0 / perf->count;
    double total = 0, min = INFINITY, max = 0;
    double median = 0, mean = 0, variance = 0;
    double c;
    size_t i;

    qsort(perf->samples, perf->count, sizeof(perf->samples[0]), &cmp_double);
    if (perf->count & 1) {
        median = perf->samples[perf->count / 2];
    }
    else {
        median = 0.5 * (perf->samples[perf->count / 2 - 1]
                        + perf->samples[perf->count / 2]);
    }

    c = 0;
    for (i = 0; i < perf->count; i++) {
        if (perf->samples[i] > max)
            max = perf->samples[i];
        if (perf->samples[i] < min)
            min = perf->samples[i];

        kbn_sumf64_r(&total, &c, perf->samples[i]);
    }
    total += c;
    mean = scale * total;

    c = 0;
    for (i = 0; i < perf->count; i++) {
        double diff;

        diff = perf->samples[i] - mean;

        kbn_sumf64_r(&variance, &c, diff * diff);
    }
    variance = scale * (variance + c);

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
