#ifndef LIBFLRL_STRUTIL_H
#define LIBFLRL_STRUTIL_H

#include "flrl/flrl.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* XXX why is compiler insisting on "gnu_printf" for these? */
__attribute__((format(gnu_printf, 2, 0)))
extern char *vmnprintf(size_t maxlen, const char *restrict fmt, va_list args);

__attribute__((format(gnu_printf, 2, 3)))
inline char *mnprintf(size_t maxlen, const char *restrict fmt, ...)
{
    va_list args;
    char *str = NULL;

    va_start(args, fmt);
    str = vmnprintf(maxlen, fmt, args);
    va_end(args);

    return str;
}

#endif
