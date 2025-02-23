#ifndef LIBFLRL_FPUTIL_H
#define LIBFLRL_FPUTIL_H

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

extern bool floats_equalish(double a, double b, double epsilon, double abs_th);

extern double kbn_sumf32v(const float *values, size_t n_values);
extern double kbn_sumf64v(const double *values, size_t n_values);

inline void kbn_sumf64_r(double *sum, double *comp, double addend)
{
    double s = *sum, c = *comp, t;

    t = s + addend;

    if (fabs(s) >= fabs(addend))
        c += (s - t) + addend;
    else
        c += (addend - t) + s;

    *sum = t;
    *comp = c;
}

#endif
