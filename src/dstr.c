#include "flrl/dstr.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_ALLOC (64)

extern inline const char *dstr_cstr(const struct dstr *dstr);
extern inline size_t dstr_len(const struct dstr *dstr);
extern inline void dstr_putc(struct dstr *dstr, int c);
extern inline void dstr_puts(struct dstr *dstr, const char *s);
extern inline void dstr_printf(struct dstr *dstr, const char *fmt, ...);
extern inline void dstr_truncate(struct dstr *dstr, size_t new_len);

/* reserve space for at least n more chars, plus \0 */
void dstr_reserve(struct dstr *dstr, size_t n)
{
    char *p;
    size_t size;

    /* better either have both buf and alloc, or neither */
    assert((dstr->buf && dstr->alloc) || (!dstr->buf && !dstr->alloc));

    /* big enough already */
    if (dstr->alloc >= dstr->count + n + 1)
        return;

    size = dstr->alloc;
    if (size == 0) size = MIN_ALLOC;

    while (size < dstr->count + n + 1)
        size += size;

    p = realloc(dstr->buf, size);
    if (!p) abort();

    dstr->buf = p;
    dstr->alloc = size;

    memset(dstr->buf + dstr->count, 0, dstr->alloc - dstr->count);
}

/* free memory used internally
 * if dstr was created with dstr_new(), use dstr_delete() instead
 */
void dstr_finish(struct dstr *dstr)
{
    free(dstr->buf);
    *dstr = DSTR_INITIALIZER;
}

/* caller takes ownership of the malloced data */
char *dstr_release(struct dstr *dstr)
{
    char *buf = dstr->buf;
    *dstr = DSTR_INITIALIZER;

    return buf;
}

/* allocate a new dstr on the heap, optionally:
 *    pre-reserving enough space for reserve chars, and
 *    pre-filling the buffer with the contents of initial
 */
struct dstr *dstr_new(size_t reserve, const char *initial)
{
    struct dstr *dstr;

    dstr = calloc(1, sizeof *dstr);
    if (!dstr) abort();

    dstr_reserve(dstr, reserve);
    if (initial)
        dstr_puts(dstr, initial);

    return dstr;
}

/* free a dstr that was allocated by dstr_new() */
void dstr_delete(struct dstr **pdstr)
{
    struct dstr *dstr;

    if (!pdstr || !*pdstr) return;

    dstr = *pdstr;
    *pdstr = NULL;

    dstr_finish(dstr);
    free(dstr);
}

void dstr_vprintf(struct dstr *dstr, const char *fmt, va_list ap)
{
    va_list ap_copy;
    size_t len;

    va_copy(ap_copy, ap);
    len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    dstr_reserve(dstr, len);
    vsnprintf(dstr->buf + dstr->count, dstr->alloc, fmt, ap);
    dstr->count += len;
}
