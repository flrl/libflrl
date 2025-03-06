#ifndef LIBFLRL_STATSUTIL_H
#define LIBFLRL_STATSUTIL_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
 #define restrict
 using std::size_t;
 extern const double statsutil_nan;
 extern void *statsutil_malloc(size_t size);
 extern void statsutil_free(void *ptr);
#endif

extern double meani8v(const int8_t *values, size_t n_values);
extern double mediani8v(const int8_t *values, size_t n_values);
extern int8_t modei8v(const int8_t *values, size_t n_values);
extern double variancei8v(const int8_t *values, size_t n_values, double mean);

extern double meanu8v(const uint8_t *values, size_t n_values);
extern double medianu8v(const uint8_t *values, size_t n_values);
extern uint8_t modeu8v(const uint8_t *values, size_t n_values);
extern double varianceu8v(const uint8_t *values, size_t n_values, double mean);

extern double meani16v(const int16_t *values, size_t n_values);
extern double mediani16v(const int16_t *values, size_t n_values);
extern int16_t modei16v(const int16_t *values, size_t n_values);
extern double variancei16v(const int16_t *values, size_t n_values, double mean);

extern double meanu16v(const uint16_t *values, size_t n_values);
extern double medianu16v(const uint16_t *values, size_t n_values);
extern uint16_t modeu16v(const uint16_t *values, size_t n_values);
extern double varianceu16v(const uint16_t *values, size_t n_values, double mean);

extern double meani32v(const int32_t *values, size_t n_values);
extern double mediani32v(const int32_t *values, size_t n_values);
extern int32_t modei32v(const int32_t *values, size_t n_values);
extern double variancei32v(const int32_t *values, size_t n_values, double mean);

extern double meanu32v(const uint32_t *values, size_t n_values);
extern double medianu32v(const uint32_t *values, size_t n_values);
extern uint32_t modeu32v(const uint32_t *values, size_t n_values);
extern double varianceu32v(const uint32_t *values, size_t n_values, double mean);

extern double meani64v(const int64_t *values, size_t n_values);
extern double mediani64v(const int64_t *values, size_t n_values);
extern int64_t modei64v(const int64_t *values, size_t n_values);
extern double variancei64v(const int64_t *values, size_t n_values, double mean);

extern double meanu64v(const uint64_t *values, size_t n_values);
extern double medianu64v(const uint64_t *values, size_t n_values);
extern uint64_t modeu64v(const uint64_t *values, size_t n_values);
extern double varianceu64v(const uint64_t *values, size_t n_values, double mean);

extern double meanf32v(const float *values, size_t n_values);
extern double medianf32v(const float *values, size_t n_values);
/* no mode for fp types */
extern double variancef32v(const float *values, size_t n_values, double mean);

extern double meanf64v(const double *values, size_t n_values);
extern double medianf64v(const double *values, size_t n_values);
/* no mode for fp types */
extern double variancef64v(const double *values, size_t n_values, double mean);

#endif
