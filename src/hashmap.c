#include "flrl/hashmap.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HASHMAP_MIN_SIZE            (8)
#define HASHMAP_MAX_SIZE            (UINT32_C(1) << 31)
#define HASHMAP_GROW_THRESHOLD      (0.65)
#define HASHMAP_SHRINK_THRESHOLD    (0.30)
#define HASHMAP_GC_THRESHOLD        (0.80)
#define HASHMAP_NO_GROW             UINT32_MAX
#define HASHMAP_NO_SHRINK           UINT32_C(0)
#define HASHMAP_NO_GC               UINT32_MAX
#define HASHMAP_BUCKET_EMPTY        UINT16_C(0)

static_assert(1 == __builtin_popcount(HASHMAP_MIN_SIZE));
static_assert(1 == __builtin_popcount(HASHMAP_MAX_SIZE));

struct hm_kmeta {
    uint32_t hash;
    uint16_t len;
    uint16_t deleted;
};
static_assert(sizeof(struct hm_kmeta) == 8);

struct hm_key {
    union {
        uint8_t kval[sizeof(void *)];
        void *kptr;
    };
};
static_assert(sizeof(((struct hm_key){}).kval)
              == sizeof(((struct hm_key){}).kptr));
#define HM_KEY(hm, i) ((hm)->kmeta[i].len <= sizeof(void *) \
                       ? (hm)->key[i].kval                  \
                       : (hm)->key[i].kptr)


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
    return hm->kmeta[index].len != HASHMAP_BUCKET_EMPTY
           && !hm->kmeta[index].deleted;
}

static inline bool should_grow(const HashMap *hm, uint32_t count)
{
    return hm->alloc < HASHMAP_MAX_SIZE
           && hm->grow_threshold != HASHMAP_NO_GROW
           && count >= hm->grow_threshold;
}

static inline bool should_shrink(const HashMap *hm, uint32_t count)
{
    return hm->alloc > HASHMAP_MIN_SIZE
           && hm->shrink_threshold != HASHMAP_NO_SHRINK
           && count < hm->shrink_threshold;
}

static inline bool should_gc(const HashMap *hm,
                             uint32_t count, uint32_t deleted)
{
    return hm->gc_threshold != HASHMAP_NO_GC
           && deleted > 0
           && count + deleted >= hm->gc_threshold;
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
        if (hm->kmeta[i].deleted) {
            if (!found_deleted) {
                found_deleted = true;
                d = i;
            }
        }
        else if (hm->kmeta[i].len == HASHMAP_BUCKET_EMPTY) {
            *pindex = found_deleted ? d : i;
            return HASHMAP_E_NOKEY;
        }
        else if (hm->kmeta[i].len == key_len
                 && hm->kmeta[i].hash == h
                 && 0 == memcmp(HM_KEY(hm, i), key, key_len))
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

    if (hm->grow_threshold == HASHMAP_NO_GROW && new_size > hm->alloc)
        return HASHMAP_E_REHASH;

    if (hm->shrink_threshold == HASHMAP_NO_SHRINK && new_size < hm->alloc)
        return HASHMAP_E_REHASH;

    if (hm->gc_threshold == HASHMAP_NO_GC && new_size == hm->alloc)
        return HASHMAP_E_REHASH;

    r = hashmap_init(&new_hm, new_size);
    if (r) return r;

    if (hm->grow_threshold == HASHMAP_NO_GROW)
        new_hm.grow_threshold = HASHMAP_NO_GROW;

    if (hm->shrink_threshold == HASHMAP_NO_SHRINK)
        new_hm.shrink_threshold = HASHMAP_NO_SHRINK;

    if (hm->gc_threshold == HASHMAP_NO_GC)
        new_hm.gc_threshold = HASHMAP_NO_GC;

    /* reuse the existing seed so we don't have to literally rehash */
    new_hm.seed = hm->seed;
    next_seed --;

    for (i = 0; i < hm->alloc; i++) {
        uint32_t new_i;

        if (!has_key_at_index(hm, i))
            continue;

        r = find(&new_hm, hm->kmeta[i].hash,
                 HM_KEY(hm, i), hm->kmeta[i].len,
                 NULL, &new_i);
        assert(r == HASHMAP_E_NOKEY); /* not found, but got a spot for it */
        assert(new_i < new_hm.alloc);

        /* steal the internals */
        new_hm.kmeta[new_i] = hm->kmeta[i];
        new_hm.key[new_i] = hm->key[i];
        new_hm.value[new_i] = hm->value[i];
        new_hm.count ++;
    }

    free(hm->kmeta);
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
    if (size > HASHMAP_MAX_SIZE) {
        memset(hm, 0, sizeof(*hm));
        return HASHMAP_E_INVALID;
    }
    else if (size < HASHMAP_MIN_SIZE)
        size = HASHMAP_MIN_SIZE;

    size = nextpow2(size);

    hm->kmeta = calloc(size, sizeof(hm->kmeta[0]));
    hm->key = calloc(size, sizeof(hm->key[0]));
    hm->value = calloc(size, sizeof(hm->value[0]));

    if (!hm->kmeta || !hm->key || !hm->value) {
        free(hm->kmeta);
        free(hm->key);
        free(hm->value);
        memset(hm, 0, sizeof(*hm));
        return HASHMAP_E_NOMEM;
    }

    hm->alloc = size;
    hm->mask = size - 1;
    hm->count = hm->deleted = 0;
    hm->seed = next_seed ++;

    hm->grow_threshold = size < HASHMAP_MAX_SIZE
                         ? (uint32_t) (size * HASHMAP_GROW_THRESHOLD) - 1
                         : HASHMAP_NO_GROW;
    hm->shrink_threshold = size > HASHMAP_MIN_SIZE
                         ? (uint32_t) (size * HASHMAP_SHRINK_THRESHOLD) - 1
                         : HASHMAP_NO_SHRINK;
    hm->gc_threshold = (uint32_t) (size * HASHMAP_GC_THRESHOLD) - 1;

    return HASHMAP_OK;
}

void hashmap_fini(HashMap *hm, void (*value_destructor)(void *))
{
    uint32_t i;

    for (i = 0; i < hm->alloc; i++) {
        if (has_key_at_index(hm, i)) {
            if (hm->kmeta[i].len > sizeof(void *))
                free(hm->key[i].kptr);

            if (value_destructor)
                value_destructor(hm->value[i]);
        }
    }

    free(hm->kmeta);
    free(hm->key);
    free(hm->value);

    memset(hm, 0, sizeof(*hm));
}

int hashmap_get(const HashMap *hm, const void *key, size_t key_len,
                void **value)
{
    uint32_t i;
    int r;

    r = find(hm, 0, key, key_len, NULL, &i);

    if (value)
        *value = (r == HASHMAP_OK) ? hm->value[i] : NULL;

    return r;
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
        if (hm->grow_threshold == HASHMAP_NO_GROW
            || (r = rehash(hm, hm->alloc * 2)))
        {
            return r;
        }
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
    else if (hm->kmeta[i].len == HASHMAP_BUCKET_EMPTY
             && should_grow(hm, hm->count))
    {
        rehash(hm, hm->alloc * 2);
        return hashmap_put(hm, key, key_len, new_value, old_value);
    }
    else if (should_gc(hm, hm->count, hm->deleted)) {
        rehash(hm, hm->alloc);
        return hashmap_put(hm, key, key_len, new_value, old_value);
    }
    else {
        if (key_len > sizeof(void *)) {
            hm->key[i].kptr = memndup(key, key_len);
            if (!hm->key[i].kptr)
                return HASHMAP_E_NOMEM;
        }
        else {
            memcpy(hm->key[i].kval, key, key_len);
        }

        if (old_value) *old_value = NULL;
        hm->deleted -= !!hm->kmeta[i].deleted;
        hm->count ++;
        hm->kmeta[i].hash = h;
        hm->kmeta[i].len = key_len;
        hm->kmeta[i].deleted = 0;
        hm->value[i] = new_value;
        return HASHMAP_OK;
    }
}

int hashmap_del(HashMap *hm, const void *key, size_t key_len, void **old_value)
{
    uint32_t i;
    int r;

    r = find(hm, 0, key, key_len, NULL, &i);

    if (r == HASHMAP_OK) {
        if (old_value) *old_value = hm->value[i];

        if (hm->kmeta[i].len > sizeof(void *))
            free(hm->key[i].kptr);

        hm->kmeta[i].hash = 0;
        hm->kmeta[i].len = 0;
        hm->kmeta[i].deleted = 1;
        hm->key[i].kptr = NULL;
        hm->value[i] = NULL;

        hm->deleted ++;
        hm->count --;

        if (should_shrink(hm, hm->count))
            rehash(hm, hm->alloc / 2);
        else if (should_gc(hm, hm->count, hm->deleted))
            rehash(hm, hm->alloc);
    }
    else {
        if (old_value) *old_value = NULL;
    }

    return r;
}

int hashmap_foreach(HashMap *hm, hashmap_foreach_cb *cb, void *ctx)
{
    uint32_t i;
    int r;

    for (i = 0; i < hm->alloc; i++) {
        if (!has_key_at_index(hm, i)) continue;

        r = cb(hm, HM_KEY(hm, i), hm->kmeta[i].len, hm->value[i], ctx);
        if (r) return r;
    }

    return 0;
}

extern inline uint32_t hashmap_hash32(const void *key, size_t key_len,
                                      uint32_t seed);
extern inline double hashmap_load_factor(const HashMap *hm);
