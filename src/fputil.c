#include "flrl/fputil.h"

#include <float.h>
#include <math.h>

bool floats_equalish(double a, double b, double epsilon, double abs_th)
{
    /* https://stackoverflow.com/a/32334103 */
    double diff, norm, ref;

    if (epsilon == 0.0f) epsilon = 128 * DBL_EPSILON;
    if (abs_th == 0.0f) abs_th = DBL_MIN;

    if (a == b) return true;

    diff = fabs(a - b);
    norm = fabs(a + b);
    if (norm > DBL_MAX) norm = DBL_MAX;

    ref = epsilon * norm;
    if (ref < abs_th) ref = abs_th;

    return diff < ref;
}

/* Kahan Babushka Neumaier sum */
extern inline void kbn_sumf64_r(double *sum, double *comp, double addend);

void noinline_kbn_sumf64_r(double *sum, double *comp, double addend)
{
    KBN_SUMF64_R(sum, comp, addend);
}

double kbn_sumf32v(const float *values, size_t n_values)
{
    double sum = 0.0, c = 0.0;
    size_t i;

    for (i = 0; i < n_values; i++)
        kbn_sumf64_r(&sum, &c, values[i]);

    return sum + c;
}

double kbn_sumf64v(const double *values, size_t n_values)
{
    double sum = 0.0, c = 0.0;
    size_t i;

    for (i = 0; i < n_values; i++)
        kbn_sumf64_r(&sum, &c, values[i]);

    return sum + c;
}

double niceceil(double x)
{
    double scale = 1.0;

    if (x == 0.0) return 0.0;

    while (fabs(x) >= 10.0) {
        x /= 10.0;
        scale *= 10.0;
    }

    while (fabs(x) < 1.0) {
        x *= 10.0;
        scale /= 10.0;
    }

    x = 0.5 * ceil(2.0 * x);

    return scale * x;
}

double nicefloor(double x)
{
    double scale = 1.0;

    if (x == 0.0) return 0.0;

    while (fabs(x) >= 10.0) {
        x /= 10.0;
        scale *= 10.0;
    }

    while (fabs(x) < 1.0) {
        x *= 10.0;
        scale /= 10.0;
    }

    x = 0.5 * floor(2.0 * x);

    return scale * x;
}
