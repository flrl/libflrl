#ifndef FLRL_HASHMAP_H
#define FLRL_HASHMAP_H

#include <stdint.h>
#include <stdlib.h>

#define HASHMAP_KEY_NULL    (UINT16_C(0))
#define HASHMAP_KEY_DELETED (UINT16_MAX)
#define HASHMAP_KEY_MAXLEN  (UINT16_MAX - 1)

typedef struct __attribute__((aligned(64))) {
    uint16_t *klen;
    uint8_t  **key;
    void     **value;
    uint32_t alloc;
    uint32_t mask;
    uint32_t count;
    uint32_t deleted;
    uint32_t seed;
    uint32_t grow_threshold;
    uint32_t shrink_threshold;
    uint32_t gc_threshold;
} HashMap;

enum {
    HASHMAP_E_NOKEY = INT_MIN,
    HASHMAP_E_KEYTOOBIG,
    HASHMAP_E_REHASH,
    HASHMAP_E_NOMEM,
    HASHMAP_E_INVALID,
    HASHMAP_E_UNKNOWN,
    HASHMAP_END_ERRORS,
    HASHMAP_OK = 0,
};

extern const char *hashmap_strerr(int e);

extern int hashmap_init(HashMap *hm, uint32_t size);
extern void hashmap_fini(HashMap *hm, void (*value_destructor)(void *));

extern void *hashmap_get(const HashMap *hm, const void *key, size_t key_len);
extern int hashmap_put(HashMap *hm, const void *key, size_t key_len,
                       void *new_value, void **old_value);

extern void *hashmap_del(HashMap *hm, const void *key, size_t key_len);

typedef int (hashmap_foreach_cb)(const void *, size_t, void *, void *);
extern int hashmap_foreach(HashMap *hm, hashmap_foreach_cb *cb, void *ctx);

inline double hashmap_load_factor(const HashMap *hm)
{
    return 1.0 * hm->count / hm->alloc;
}
#endif
