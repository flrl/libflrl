#ifndef LIBFLRL_BASE64_H
#define LIBFLRL_BASE64_H

#include "flrl/flrl.h"

#include <stddef.h>

/* like strlen -- number of chars excluding terminating \0 */
inline size_t base64_encoded_len(size_t len)
{
    return 4 * ((len + 2) / 3);
}

extern ssize_t base64_decoded_len(const char *encoded);

extern const char *base64_encode(char *dest, size_t dest_len,
                                 const void *src, size_t src_len);
extern ssize_t base64_decode(void *dest, size_t dest_len,
                             const char *src, size_t src_len);

#endif
