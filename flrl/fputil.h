#ifndef LIBFLRL_FPUTIL_H
#define LIBFLRL_FPUTIL_H

#include <stdbool.h>
#include <stdlib.h>

extern bool floats_equalish(double a, double b, double epsilon, double abs_th);

double kbn_sumf32v(const float *values, size_t n_values);
double kbn_sumf64v(const double *values, size_t n_values);

#endif
