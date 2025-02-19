#include "flrl/hashmap.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HASHMAP_MIN_SIZE            (8)
#define HASHMAP_GROW_THRESHOLD      (0.65)
#define HASHMAP_SHRINK_THRESHOLD    (0.30)
#define HASHMAP_GC_THRESHOLD        (0.80)

static uint32_t next_seed = 1;

static inline uint32_t nextpow2(uint32_t v)
{
    /* https://graphics.stanford.edu/%7Eseander/bithacks.html#RoundUpPowerOf2 */
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static inline void *memndup(const void *a, size_t len)
{
    void *p = malloc(len);
    if (!p) return NULL;

    memcpy(p, a, len);
    return p;
}

static inline bool has_key_at_index(const HashMap *hm, uint32_t index)
{
    return hm->klen[index] > HASHMAP_KEY_NULL
           && hm->klen[index] < HASHMAP_KEY_DELETED;
}

static int find(const HashMap *hm,
                uint32_t known_hash,
                const void *key, size_t key_len,
                uint32_t *phash,
                uint32_t *pindex)
{
    uint32_t d, h, i, x;
    bool found_deleted = false;

    if (!key || !key_len)
        return HASHMAP_E_INVALID;
    if (key_len > HASHMAP_KEY_MAXLEN)
        return HASHMAP_E_KEYTOOBIG;

    h = known_hash ? known_hash : hashmap_hash32(key, key_len, hm->seed);
    if (phash) *phash = h;

    i = x = h & hm->mask;
    do {
        if (hm->klen[i] == HASHMAP_KEY_NULL) {
            *pindex = found_deleted ? d : i;
            return HASHMAP_E_NOKEY;
        }
        else if (hm->klen[i] == HASHMAP_KEY_DELETED && !found_deleted) {
            found_deleted = true;
            d = i;
        }
        else if (hm->klen[i] == key_len
                 && hm->hash[i] == h
                 && 0 == memcmp(hm->key[i], key, key_len))
        {
            *pindex = i;
            return HASHMAP_OK;
        }

        i = (i + 1) & hm->mask;
    } while (i != x);

    /* if we get here without having found a deleted kv, then the map is full,
     * and we ought to have rehashed earlier!
     */
    if (!found_deleted) {
        *pindex = UINT32_MAX;
        return HASHMAP_E_REHASH;
    }

    *pindex = d;
    return HASHMAP_E_NOKEY;
}

static int rehash(HashMap *hm, uint32_t new_size)
{
    HashMap new_hm;
    uint32_t i;
    int r;

    assert(new_size >= hm->count);
    if (new_size < hm->count) return HASHMAP_E_INVALID;

    if (new_size < HASHMAP_MIN_SIZE && hm->alloc == HASHMAP_MIN_SIZE)
        return HASHMAP_OK;

    r = hashmap_init(&new_hm, new_size);
    if (r) return r;

    /* reuse the existing seed so we don't have to literally rehash */
    new_hm.seed = hm->seed;
    next_seed --;

    for (i = 0; i < hm->alloc; i++) {
        uint32_t new_i;

        if (!has_key_at_index(hm, i))
            continue;

        r = find(&new_hm, hm->hash[i], hm->key[i], hm->klen[i], NULL, &new_i);
        assert(r == HASHMAP_E_NOKEY); /* not found, but got a spot for it */
        assert(new_i < new_hm.alloc);

        /* steal the internals */
        new_hm.hash[new_i] = hm->hash[i];
        new_hm.klen[new_i] = hm->klen[i];
        new_hm.key[new_i] = hm->key[i];
        new_hm.value[new_i] = hm->value[i];
        new_hm.count ++;
    }

    free(hm->klen);
    free(hm->key);
    free(hm->value);
    memcpy(hm, &new_hm, sizeof(*hm));
    return HASHMAP_OK;
}

const char *hashmap_strerr(int e)
{
    static char buf[64] = {0};
    int r;

    switch (e) {
    case HASHMAP_E_NOKEY:
        return "key not found";
    case HASHMAP_E_KEYTOOBIG:
        return "key too long";
    case HASHMAP_E_REHASH:
        return "map is full";
    case HASHMAP_E_NOMEM:
        return "memory allocation failed";
    case HASHMAP_E_INVALID:
        return "invalid argument";
    case HASHMAP_E_UNKNOWN:
        return "unknown error";
    case HASHMAP_OK:
        return "ok";
    default:
        r = snprintf(buf, sizeof(buf), "unrecognised error code %d", e);
        assert(r < (int) sizeof(buf));
        return buf;
    }
}

int hashmap_init(HashMap *hm, uint32_t size)
{
    if (size < HASHMAP_MIN_SIZE)
        size = HASHMAP_MIN_SIZE;
    size = nextpow2(size);

    hm->hash = calloc(size, sizeof(hm->hash[0]));
    hm->klen = calloc(size, sizeof(hm->klen[0]));
    hm->key = calloc(size, sizeof(hm->key[0]));
    hm->value = calloc(size, sizeof(hm->value[0]));

    if (!hm->hash || !hm->klen || !hm->key || !hm->value) {
        free(hm->hash);
        free(hm->klen);
        free(hm->key);
        free(hm->value);
        memset(hm, 0, sizeof(*hm));
        return HASHMAP_E_NOMEM;
    }

    hm->alloc = size;
    hm->mask = size - 1;
    hm->count = hm->deleted = 0;
    hm->seed = next_seed ++;
    hm->grow_threshold = (uint32_t) (size * HASHMAP_GROW_THRESHOLD) - 1;
    hm->shrink_threshold = (uint32_t) (size * HASHMAP_SHRINK_THRESHOLD) - 1;
    hm->gc_threshold = (uint32_t) (size * HASHMAP_GC_THRESHOLD) - 1;

    return HASHMAP_OK;
}

void hashmap_fini(HashMap *hm, void (*value_destructor)(void *))
{
    uint32_t i;

    for (i = 0; i < hm->alloc; i++) {
        if (has_key_at_index(hm, i)) {
            free(hm->key[i]);

            if (value_destructor)
                value_destructor(hm->value[i]);
        }
    }

    free(hm->hash);
    free(hm->klen);
    free(hm->key);
    free(hm->value);

    memset(hm, 0, sizeof(*hm));
}

void *hashmap_get(const HashMap *hm, const void *key, size_t key_len)
{
    uint32_t i;
    int r;

    r = find(hm, 0, key, key_len, NULL, &i);

    return r == HASHMAP_OK ? hm->value[i] : NULL;
}

int hashmap_put(HashMap *hm,
                const void *key, size_t key_len,
                void *new_value,
                void **old_value)
{
    uint32_t h, i;
    int r;

    r = find(hm, 0, key, key_len, &h, &i);

    if (r == HASHMAP_E_REHASH) {
        if ((r = rehash(hm, hm->alloc * 2)))
            return r;
        return hashmap_put(hm, key, key_len, new_value, old_value);
    }
    else if (r == HASHMAP_OK) {
        assert(has_key_at_index(hm, i));

        if (new_value != hm->value[i]) {
            if (old_value) *old_value = hm->value[i];
            hm->value[i] = new_value;
        }
        else {
            if (old_value) *old_value = NULL;
        }
        return HASHMAP_OK;
    }
    else if (r != HASHMAP_E_NOKEY) {
        return r;
    }
    else if (hm->klen[i] == HASHMAP_KEY_NULL
             && hm->count >= hm->grow_threshold)
    {
        rehash(hm, hm->alloc * 2);
        return hashmap_put(hm, key, key_len, new_value, old_value);
    }
    else if (hm->count + hm->deleted >= hm->gc_threshold) {
        rehash(hm, hm->alloc);
        return hashmap_put(hm, key, key_len, new_value, old_value);
    }
    else {
        hm->key[i] = memndup(key, key_len);
        if (!hm->key[i])
            return HASHMAP_E_NOMEM;

        if (old_value) *old_value = NULL;
        hm->deleted -= (hm->klen[i] == HASHMAP_KEY_DELETED);
        hm->count ++;
        hm->hash[i] = h;
        hm->klen[i] = key_len;
        hm->value[i] = new_value;
        return HASHMAP_OK;
    }
}

void *hashmap_del(HashMap *hm, const void *key, size_t key_len)
{
    void *old_value = NULL;
    uint32_t i;
    int r;

    r = find(hm, 0, key, key_len, NULL, &i);

    if (r == HASHMAP_OK) {
        old_value = hm->value[i];

        free(hm->key[i]);
        hm->hash[i] = 0;
        hm->klen[i] = HASHMAP_KEY_DELETED;
        hm->key[i] = NULL;
        hm->value[i] = NULL;

        hm->deleted ++;
        hm->count --;
    }

    if (hm->count < hm->shrink_threshold)
        rehash(hm, hm->alloc / 2);
    else if (hm->count + hm->deleted >= hm->gc_threshold)
        rehash(hm, hm->alloc);

    return old_value;
}

int hashmap_foreach(HashMap *hm, hashmap_foreach_cb *cb, void *ctx)
{
    uint32_t i;
    int r;

    for (i = 0; i < hm->alloc; i++) {
        if (!has_key_at_index(hm, i)) continue;

        r = cb(hm->key[i], hm->klen[i], hm->value[i], ctx);
        if (r) return r;
    }

    return 0;
}

extern inline uint32_t hashmap_hash32(const void *key, size_t key_len,
                                      uint32_t seed);
extern inline double hashmap_load_factor(const HashMap *hm);
