#ifndef LIBFLRL_PERF_H
#define LIBFLRL_PERF_H

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <profileapi.h>
typedef LARGE_INTEGER perf_raw_time;
#else
#error "not implemented for non-windows yet..."
#endif

struct perf {
    perf_raw_time started;
#ifdef _WIN32
    perf_raw_time accum;
    size_t n_accum;
#endif
    char *name;
    size_t alloc;
    size_t count;
    size_t next;
    double samples[];
};

extern struct perf *perf_new(const char *name, size_t max_samples);
extern void perf_free(struct perf *perf);

inline void perf_start(struct perf *perf)
{
#ifdef _WIN32
    QueryPerformanceCounter(&perf->started);
#endif
}

extern void perf_add_sample(struct perf *perf, perf_raw_time ended);

inline void perf_end(struct perf *perf)
{
    perf_raw_time ended;

#ifdef _WIN32
    QueryPerformanceCounter(&ended);
#endif

    perf_add_sample(perf, ended);
}

extern void perf_report(FILE *out, const char *title,
                        struct perf **perfs, size_t n_perfs);

#endif
