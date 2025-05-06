#include "flrl/perf.h"

#include "flrl/fputil.h"
#include "flrl/statsutil.h"

#include <float.h>
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
        /* n.b. expected minimum resolution of 100 ns because this always(?)
         * return 10,000,000 ticks per second
         */
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

static inline void finish_sample(const struct perf *perf)
{
    size_t next = perf->next;
    struct perf *backdoor = (struct perf *) perf;

    backdoor->samples[next] = get_elapsed(backdoor);
    backdoor->accum.QuadPart = 0;
    backdoor->n_accum = 0;

    next = (next + 1) % backdoor->alloc;
    backdoor->next = next;
    if (backdoor->count < backdoor->alloc) backdoor->count ++;
}

void perf_add_sample(struct perf *perf, perf_raw_time ended)
{
    accumulate(perf, ended);

    if (perf->accum.QuadPart >= min_ticks) {
        finish_sample(perf);
    }
}

static const wchar_t *format_sample(wchar_t buf[11], double seconds)
{
    const wchar_t *const suffix[] = { L"ns", L"µs", L"ms", L"s", L"m", L"h", L"d" };
    const double divisor[] =        { 1000.0, 1000.0, 1000.0, 60.0, 60.0, 24.0 };
    const unsigned n_divisors = sizeof(divisor) / sizeof(divisor[0]);
    const wchar_t *fmt;
    unsigned d = 0;

    seconds *= 1000000000.0;

    while (d < n_divisors && seconds >= 1.5 * divisor[d]) {
        seconds = seconds / divisor[d++];
    }

    if (seconds >= 10000.0)
        fmt = L"%.0e%ls";
    else if (seconds >= 1000.0)
        fmt = L"%5.0f%ls";
    else if (seconds < 1.0)
        fmt = L"%5.3f%ls";
    else
        fmt = L"%#5.4g%ls";

    swprintf(buf, 11, fmt, seconds, suffix[d]);
    return buf;
}

void perf_report(FILE *out, const char *title,
                 struct perf **perfs, size_t n_perfs)
{
    struct boxplot *boxplots = NULL;
    size_t i;

    boxplots = calloc(n_perfs, sizeof(boxplots[0]));
    if (!boxplots) return;

    for (i = 0; i < n_perfs; i++) {
        const struct perf *perf = perfs[i];
        struct boxplot *bp = &boxplots[i];

        if (perf->n_accum) {
            finish_sample(perf);
        }

        bp->label = perf->name;
        summary7f64v(perf->samples, perf->count, bp->quantiles, FENCE_PERC2);
    }
    boxplot_print(title, boxplots, n_perfs, &format_sample, FENCE_PERC2, out);

    free(boxplots);
}
