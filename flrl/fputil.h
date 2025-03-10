#ifndef LIBFLRL_FPUTIL_H
#define LIBFLRL_FPUTIL_H

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
 /* work around math header not being available in freestanding c++ */
 #define INLINE_MATH (0)
#else
 #include <math.h>
 #define INLINE_MATH (1)
#endif

extern bool floats_equalish(double a, double b, double epsilon, double abs_th);

extern double kbn_sumf32v(const float *values, size_t n_values);
extern double kbn_sumf64v(const double *values, size_t n_values);

#define KBN_SUMF64_R(sum, comp, addend) do {            \
    double *sum_ = (sum), *comp_ = (comp);              \
    double s = *sum_, c = *comp_, a = (addend), t;      \
    t = s + a;                                          \
    if (fabs(s) >= fabs(a))                             \
        c += (s - t) + a;                               \
    else                                                \
        c += (a - t) + s;                               \
    *sum_ = t;                                          \
    *comp_ = c;                                         \
} while (0);

extern void noinline_kbn_sumf64_r(double *sum, double *comp, double addend);

#if INLINE_MATH
inline void kbn_sumf64_r(double *sum, double *comp, double addend)
{
    KBN_SUMF64_R(sum, comp, addend);
}
#else
#define kbn_sumf64_r(sum, comp, addend) \
    noinline_kbn_sumf64_r(sum, comp, addend);
#endif

#endif
