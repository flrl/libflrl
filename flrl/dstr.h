#ifndef LIBFLRL_DSTR_H
#define LIBFLRL_DSTR_H

#include <stddef.h>

/* want to be able to allocate these on the stack */
struct dstr {
    char *buf;
    size_t alloc;
    size_t count;
};

#define DSTR_INITIALIZER (struct dstr){0}

void dstr_reserve(struct dstr *dstr, size_t size);
void dstr_finish(struct dstr *dstr);
char *dstr_release(struct dstr *dstr);

struct dstr *dstr_new(const char *initial, size_t size);
void dstr_delete(struct dstr **pdstr);

void dstr_putc(struct dstr *dstr, int c);
void dstr_puts(struct dstr *dstr, size_t len, const char *s);
void dstr_printf(struct dstr *dstr, const char *fmt, ...)
    __attribute__((format(gnu_printf,2,3)));
void dstr_vprintf(struct dstr *dstr, const char *fmt, va_list ap)
    __attribute__((format(gnu_printf,2,0)));

void dstr_reset(struct dstr *dstr);

inline const char *dstr_cstr(const struct dstr *dstr) { return dstr->buf; }
inline size_t dstr_len(const struct dstr *dstr) { return dstr->count; }

#endif
