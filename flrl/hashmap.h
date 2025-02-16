#ifndef FLRL_HASHMAP_H
#define FLRL_HASHMAP_H

#include <stdint.h>
#include <stdlib.h>

#define HASHMAP_MAX_KEYLEN  UINT16_MAX

struct hm_kv;

typedef struct __attribute__((aligned(64))) {
    struct hm_kv *kvs;
    uint32_t alloc;
    uint32_t count;
    uint32_t deleted;
    uint32_t mask;
    uint32_t seed;
    uint32_t grow_threshold;
    uint32_t shrink_threshold;
    uint32_t gc_threshold;
} HashMap;

extern int hashmap_init(HashMap *hm, uint32_t size);
extern void hashmap_fini(HashMap *hm, void (*value_destructor)(void *));

extern void *hashmap_get(const HashMap *hm, const void *key, size_t key_len);
extern void *hashmap_put(HashMap *hm, const void *key, size_t key_len, void *value);

extern void *hashmap_del(HashMap *hm, const void *key, size_t key_len);

typedef int (hashmap_foreach_cb)(const void *, size_t, void *, void *);
extern int hashmap_foreach(HashMap *hm, hashmap_foreach_cb *cb, void *ctx);

inline double hashmap_load_factor(const HashMap *hm)
{
    return 1.0 * hm->count / hm->alloc;
}
#endif
