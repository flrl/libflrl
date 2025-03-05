#include "flrl/statsutil.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

const double statsutil_nan = NAN;

void *statsutil_malloc(size_t size)
{
    return malloc(size);
}

void statsutil_free(void *ptr)
{
    free(ptr);
}


