#ifndef FLRL_HASHMAP_H
#define FLRL_HASHMAP_H

#include <stdint.h>
#include <stdlib.h>

#define HASHMAP_KEY_MAXLEN  (UINT16_MAX)

typedef struct __attribute__((aligned(64))) {
    struct hm_kmeta *kmeta;
    struct hm_key *key;
    void **value;
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

extern int hashmap_get(const HashMap *hm, const void *key, size_t key_len,
                       void **value);
extern int hashmap_put(HashMap *hm, const void *key, size_t key_len,
                       void *new_value, void **old_value);
extern int hashmap_del(HashMap *hm, const void *key, size_t key_len,
                       void **old_value);

typedef int (hashmap_foreach_cb)(const void *, size_t, void *, void *);
extern int hashmap_foreach(HashMap *hm, hashmap_foreach_cb *cb, void *ctx);

inline uint32_t hashmap_hash32(const void *key, size_t key_len, uint32_t seed)
{
    /* bob jenkins' one-at-a-time hash will do for now, but
     * XXX consider using lookup3 instead
     */
    const uint8_t *k = key;
    uint32_t h = seed;
    size_t i;

    for (i = 0; i < key_len; i++) {
        h += k[i];
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return h;
}

inline double hashmap_load_factor(const HashMap *hm)
{
    return 1.0 * hm->count / hm->alloc;
}
#endif
