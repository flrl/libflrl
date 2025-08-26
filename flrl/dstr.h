#ifndef LIBFLRL_DSTR_H
#define LIBFLRL_DSTR_H

#include "flrl/flrl.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

/* want to be able to allocate these on the stack */
struct dstr {
    char *buf;
    size_t alloc;
    size_t count;
};

#define DSTR_INITIALIZER (struct dstr){0}

extern void dstr_reserve(struct dstr *dstr, size_t size);
extern void dstr_finish(struct dstr *dstr);
extern char *dstr_release(struct dstr *dstr);

extern struct dstr *dstr_new(size_t reserve, const char *initial);
extern void dstr_delete(struct dstr **pdstr);

extern void dstr_vprintf(struct dstr *dstr, const char *fmt, va_list ap)
    __attribute__((format(gnu_printf,2,0)));

inline const char *dstr_cstr(const struct dstr *dstr)
{
    return dstr->buf;
}

inline size_t dstr_len(const struct dstr *dstr)
{
    return dstr->count;
}

inline void dstr_putc(struct dstr *dstr, int c)
{
    dstr_reserve(dstr, 1);
    dstr->buf[dstr->count++] = (char) c;
}

inline void dstr_puts(struct dstr *dstr, const char *s)
{
    size_t len = strlen(s);

    dstr_reserve(dstr, len);
    memcpy(dstr->buf + dstr->count, s, len);
    dstr->count += len;
}

__attribute__((format(gnu_printf,2,3)))
inline void dstr_printf(struct dstr *dstr, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    dstr_vprintf(dstr, fmt, ap);
    va_end(ap);
}

inline void dstr_truncate(struct dstr *dstr, size_t new_len)
{
    if (new_len > dstr->count) {
        dstr_reserve(dstr, new_len - dstr->count);
        memset(dstr->buf + dstr->count, 0, new_len - dstr->count);
    }
    else if (new_len < dstr->count) {
        memset(dstr->buf + new_len, 0, dstr->count - new_len);
    }

    dstr->count = new_len;
}

#endif
