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
double kbn_sumf32v(const float *values, size_t n_values)
{
    double sum = 0.0, c = 0.0;
    size_t i;

    for (i = 0; i < n_values; i++) {
        double t = sum + values[i];

        if (fabs(sum) >= fabs(values[i]))
            c += (sum - t) + values[i];
        else
            c += (values[i] - t) + sum;

        sum = t;
    }

    return sum + c;
}

double kbn_sumf64v(const double *values, size_t n_values)
{
    double sum = 0.0, c = 0.0;
    size_t i;

    for (i = 0; i < n_values; i++) {
        double t = sum + values[i];

        if (fabs(sum) >= fabs(values[i]))
            c += (sum - t) + values[i];
        else
            c += (values[i] - t) + sum;

        sum = t;
    }

    return sum + c;
}
