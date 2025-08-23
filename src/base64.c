#include "flrl/base64.h"

#include "flrl/xassert.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern inline size_t base64_encoded_len(size_t len);

static const char encode_map[] = "ABCD" "EFGH" "IJKL" "MNOP"
                                 "QRST" "UVWX" "YZab" "cdef"
                                 "ghij" "klmn" "opqr" "stuv"
                                 "wxyz" "0123" "4567" "89-_";

#define WS (64) /* whitespace */
#define PD (65) /* padding */
#define XX (66) /* invalid */

static const uint8_t decode_map[] = {
/*  00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f        */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, WS, WS, WS, WS, WS, XX, XX, /* 00 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* 10 */
    WS, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, 62, XX, 62, XX, 63, /* 20 */
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, XX, XX, XX, PD, XX, XX, /* 30 */
    XX,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 40 */
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, XX, XX, XX, XX, 63, /* 50 */
    XX, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 60 */
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, XX, XX, XX, XX, XX, /* 70 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* 80 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* 90 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* a0 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* b0 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* c0 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* d0 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* e0 */
    XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, /* f0 */
};

const char *base64_encode(char *dest, size_t dest_len,
                          const void *src_orig, size_t src_len)
{
    const uint8_t *src = (const uint8_t *) src_orig;
    size_t i, padding;
    uint32_t three_bytes;
    uint8_t bytes[4];
    char *p = dest;

    if (dest_len < 1 + base64_encoded_len(src_len)) return NULL;

    for (i = 0; i < src_len; i += 3) {
        three_bytes = ((uint32_t) src[i]) << 16;
        if (i + 1 < src_len)
            three_bytes |= ((uint32_t) src[i + 1]) << 8;
        if (i + 2 < src_len)
            three_bytes |= src[i + 2];

        bytes[0] = (uint8_t)(three_bytes >> 18) & 0x3f;
        bytes[1] = (uint8_t)(three_bytes >> 12) & 0x3f;
        bytes[2] = (uint8_t)(three_bytes >> 6)  & 0x3f;
        bytes[3] = (uint8_t)(three_bytes)       & 0x3f;

        /* always at least two dest chars per src byte */
        *p++ = encode_map[bytes[0]];
        *p++ = encode_map[bytes[1]];

        /* a third dest char if there was a second src byte */
        if (i + 1 < src_len)
            *p++ = encode_map[bytes[2]];

        /* a fourth dest char if there was a third src byte */
        if (i + 2 < src_len)
            *p++ = encode_map[bytes[3]];
    }

    padding = (src_len % 3)
            ? 3 - (src_len % 3)
            : 0;
    for (i = 0; i < padding; i++)
        *p++ = '=';

    *p = '\0';
    return dest;
}

ssize_t base64_decoded_len(const char *encoded)
{
    const unsigned char *p = (const unsigned char *) encoded;
    size_t encoded_len = 0;
    size_t fours, rem;

    while (p && *p) {
        uint8_t c = decode_map[*p++];

        switch (c) {
        case WS:
            continue;
        case PD:
            p = NULL;
            break;
        case XX:
            return -1;
        default:
            encoded_len ++;
        }
    }

    if (encoded_len < 2) return -1; /* invalid */

    fours = encoded_len / 4;
    rem = encoded_len % 4;

    switch (rem) {
    case 0:
        return fours * 3;
    case 2:
        return fours * 3 + 1;
    case 3:
        return fours * 3 + 2;
    default:
        return -1; /* invalid! */
    }
}

ssize_t base64_decode(void *dest_orig, size_t dest_len,
                      const char *src_orig, size_t src_len)
{
    const unsigned char *src = (const unsigned char *) src_orig;
    const unsigned char *end;
    uint8_t *dest = (uint8_t *) dest_orig;
    uint8_t *p = dest;
    uint32_t buf = 0;
    ssize_t expected_len;
    size_t len = 0;
    int i = 0;

    if (src_len == 0)
        src_len = strlen(src_orig);
    end = src + src_len;

    expected_len = base64_decoded_len(src_orig);
    if (expected_len == -1 || (size_t) expected_len > dest_len) return -1;

    while (src < end) {
        uint8_t c = decode_map[*src++];

        switch (c) {
        case WS:
            continue;
        case XX:
            return -1;
        case PD:
            src = end;
            continue;
        default:
            buf = buf << 6 | c;
            if (++i == 4) {
                len += 3;
                if (!xassert(len <= dest_len)) return -1;
                *p++ = (buf >> 16)  & 0xff;
                *p++ = (buf >> 8)   & 0xff;
                *p++ = (buf)        & 0xff;
                buf = 0;
                i = 0;
            }
        }
    }

    if (i == 3) {
        len += 2;
        if (!xassert(len <= dest_len)) return -1;
        *p++ = (buf >> 10) & 0xff;
        *p++ = (buf >> 2)  & 0xff;
    }
    else if (i == 2) {
        len ++;
        if (!xassert(len <= dest_len)) return -1;
        *p++ = (buf >> 4) & 0xff;
    }

    return len;
}
