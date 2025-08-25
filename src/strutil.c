#include "flrl/strutil.h"

#include <stdio.h>

extern inline char *mnprintf(size_t maxlen, const char *restrict fmt, ...);

char *vmnprintf(size_t maxlen, const char *restrict fmt, va_list args)
{
    va_list args_copy;
    size_t len;
    char *str = NULL;

    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len > maxlen) len = maxlen;

    len ++;
    str = malloc(len);
    if (!str) return NULL;

    vsnprintf(str, len, fmt, args);
    return str;
}
