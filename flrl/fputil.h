#ifndef LIBFLRL_FPUTIL_H
#define LIBFLRL_FPUTIL_H

#include <stdlib.h>

extern int floats_equalish(float a, float b, float epsilon, float abs_th);

double kbn_sumf32v(const float *values, size_t n_values);
double kbn_sumf64v(const double *values, size_t n_values);

#endif
