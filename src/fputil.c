#include <float.h>
#include <math.h>

#include "flrl/fputil.h"

int floats_equalish(float a, float b, float epsilon, float abs_th)
{
    /* https://stackoverflow.com/a/32334103 */
    float diff, norm, ref;

    if (epsilon == 0.0f) epsilon = 128 * FLT_EPSILON;
    if (abs_th == 0.0f) abs_th = FLT_MIN;

    if (a == b) return 1;

    diff = fabsf(a - b);
    norm = fabsf(a + b);
    if (norm > FLT_MAX) norm = FLT_MAX;

    ref = epsilon * norm;
    if (ref < abs_th) ref = abs_th;

    if (diff < ref) return 1;

    return 0;
}
