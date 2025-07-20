#ifndef FLRL_HASHMAP_H
#define FLRL_HASHMAP_H

#include "flrl/statsutil.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define HASHMAP_MAX_KEYLEN  (UINT8_MAX)
#define HASHMAP_NO_GROW     UINT32_MAX
#define HASHMAP_NO_SHRINK   UINT32_C(0)

typedef struct __attribute__((aligned(64))) {
    struct hm_key *key;
    void **value;
    uint32_t *hash;
    uint32_t alloc;
    uint32_t count;
    uint32_t max_psl;
    uint32_t seed;
    uint32_t grow_threshold;
    uint32_t shrink_threshold;
} HashMap;

typedef struct {
    struct {
        struct summary7 summary7;
        double mean;
        double variance;
        size_t n_samples;
    } psl;
    struct {
        struct summary7 summary7;
        double mean;
        double variance;
        size_t n_samples;
    } bdc;
    struct {
        struct summary7 summary7;
        double mean;
        double variance;
        size_t n_samples;
    } keylen;
    double load;
} HashMapStats;

enum {
    HASHMAP_E_NOKEY = INT_MIN,
    HASHMAP_E_KEYTOOBIG,
    HASHMAP_E_RESIZE,
    HASHMAP_E_NOMEM,
    HASHMAP_E_INVALID,
    HASHMAP_E_UNKNOWN,
    HASHMAP_END_ERRORS,
    HASHMAP_OK = 0,
};

__attribute__((const))
extern const char *hashmap_strerr(int e);

extern int hashmap_init(HashMap *hm, uint32_t size);
extern void hashmap_fini(HashMap *hm, void (*value_destructor)(void *));
extern int hashmap_resize(HashMap *hm, uint32_t new_size);

extern int hashmap_get(const HashMap *hm, const void *key, size_t key_len,
                       void **value);
extern int hashmap_put(HashMap *hm, const void *key, size_t key_len,
                       void *new_value, void **old_value);
extern int hashmap_del(HashMap *hm, const void *key, size_t key_len,
                       void **old_value);

typedef int (hashmap_mod_cb)(const HashMap *hm,
                             const void *key, size_t key_len,
                             void **value,
                             void *ctx);

extern int hashmap_mod(HashMap *hm, const void *key, size_t key_len,
                       void *init_value,
                       hashmap_mod_cb *mod_cb, void *mod_ctx);

typedef int (hashmap_foreach_cb)(const HashMap *hm,
                                 const void *key,
                                 size_t key_len,
                                 void *value,
                                 void *ctx);
extern int hashmap_foreach(const HashMap *hm, hashmap_foreach_cb *cb, void *ctx);

extern void hashmap_get_stats(const HashMap *hm, HashMapStats *hs);

struct randbs;
/* pkey will be assigned a malloced copy of the chosen key, caller must free */
extern int hashmap_random(const HashMap *hm, struct randbs *rbs,
                          void **pkey, size_t *pkey_len, void **pvalue);

__attribute__((pure))
inline uint32_t hashmap_hash32(const void *key, size_t key_len, uint32_t seed)
{
    /* bob jenkins' one-at-a-time hash will do for now, but
     * XXX consider using lookup3 instead
     */
    const uint8_t *k = (const uint8_t *) key;
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

#endif
