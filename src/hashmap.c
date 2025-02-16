#include "flrl/hashmap.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define HASHMAP_MIN_SIZE            (8)
#define HASHMAP_GROW_THRESHOLD      (0.65)
#define HASHMAP_SHRINK_THRESHOLD    (0.30)
#define HASHMAP_GC_THRESHOLD        (0.80)

struct hm_key {
    uint16_t len;
    uint8_t bytes[0];
};
#define HM_KEY_NULL     ((const struct hm_key *) 0)
#define HM_KEY_DELETED  ((const struct hm_key *) 1)

struct hm_kv {
    const struct hm_key *key;
    void *value;
} __attribute__((aligned));

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

static inline uint32_t hash32(const void *key, size_t key_len, uint32_t seed)
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

static inline struct hm_key *make_key(const void *key, size_t key_len)
{
    struct hm_key *hm_key;

    assert(key && key_len && key_len <= HASHMAP_MAX_KEYLEN);
    if (!key || !key_len || key_len > HASHMAP_MAX_KEYLEN) return NULL;

    hm_key = malloc(sizeof(*hm_key) + key_len);
    if (!hm_key) return NULL;

    hm_key->len = key_len;
    memcpy(hm_key->bytes, key, key_len);

    return hm_key;
}

static inline bool hm_key_valid(const struct hm_key *key)
{
    return key > HM_KEY_DELETED;
}

static struct hm_kv *find(const HashMap *hm, const void *key, size_t key_len)
{
    uint32_t h, i, x;
    struct hm_kv *found_deleted = NULL;

    h = hash32(key, key_len, hm->seed);

    i = x = h & hm->mask;
    do {
        struct hm_kv *kv = &hm->kvs[i];

        if (kv->key == HM_KEY_NULL)
            return found_deleted ? found_deleted : kv;

        if (kv->key == HM_KEY_DELETED) {
            found_deleted = &hm->kvs[i];
        }
        else if (kv->key->len == key_len
                 && 0 == memcmp(kv->key->bytes, key, key_len))
        {
            return kv;
        }

        i = (i + 1) & hm->mask;
    } while (i != x);

    /* if we get here without having found a deleted kv, then the map is full,
     * and we ought to have rehashed earlier!
     */
    return found_deleted;
}

static int rehash(HashMap *hm, uint32_t new_size)
{
    HashMap new_hm;
    uint32_t i;
    int r;

    assert(new_size >= hm->count);
    if (new_size < hm->count) return -1;

    if (new_size < HASHMAP_MIN_SIZE && hm->alloc == HASHMAP_MIN_SIZE)
        return 0;

    r = hashmap_init(&new_hm, new_size);
    if (r) return r;

    for (i = 0; i < hm->alloc; i++) {
        struct hm_kv *kv = &hm->kvs[i];
        struct hm_kv *new_kv;

        if (!hm_key_valid(kv->key))
            continue;

        new_kv = find(&new_hm, kv->key->bytes, kv->key->len);
        assert(new_kv && hm_key_valid(new_kv->key));

        /* steal the internals */
        new_kv->key = kv->key;
        new_kv->value = kv->value;
        new_hm.count ++;
    }

    free(hm->kvs);
    memcpy(hm, &new_hm, sizeof(*hm));
    return 0;
}

int hashmap_init(HashMap *hm, uint32_t size)
{
    if (size < HASHMAP_MIN_SIZE)
        size = HASHMAP_MIN_SIZE;
    else
        size = nextpow2(size);

    hm->kvs = calloc(size, sizeof(hm->kvs[0]));
    if (!hm->kvs) return -1;

    hm->alloc = size;
    hm->count = hm->deleted = 0;

    hm->mask = size - 1;
    hm->seed = next_seed ++;

    hm->grow_threshold = (uint32_t) (size * HASHMAP_GROW_THRESHOLD) - 1;
    hm->shrink_threshold = (uint32_t) (size * HASHMAP_SHRINK_THRESHOLD) - 1;
    hm->gc_threshold = (uint32_t) (size * HASHMAP_GC_THRESHOLD) - 1;

    return 0;
}

void hashmap_fini(HashMap *hm, void (*value_destructor)(void *))
{
    uint32_t i;

    for (i = 0; i < hm->alloc; i++) {
        struct hm_kv *kv = &hm->kvs[i];
        if (hm_key_valid(kv->key)) {
            free((struct hm_key *) kv->key);

            if (value_destructor)
                value_destructor(kv->value);
        }
    }
    free(hm->kvs);
    memset(hm, 0, sizeof(*hm));
}

void *hashmap_get(const HashMap *hm, const void *key, size_t key_len)
{
    struct hm_kv *kv;

    kv = find(hm, key, key_len);
    return kv && hm_key_valid(kv->key) ? kv->value : NULL;
}

void *hashmap_put(HashMap *hm, const void *key, size_t key_len, void *value)
{
    void *old_value = NULL;
    struct hm_kv *kv;

    kv = find(hm, key, key_len);

    if (!kv) {
        if (rehash(hm, hm->alloc * 2))
            return NULL;
        return hashmap_put(hm, key, key_len, value);
    }
    else if (hm_key_valid(kv->key)) {
        if (kv->value != value) {
            old_value = kv->value;
            kv->value = value;
        }
        return old_value;
    }
    else if (kv->key == HM_KEY_NULL && hm->count >= hm->grow_threshold) {
        rehash(hm, hm->alloc * 2);
        return hashmap_put(hm, key, key_len, value);
    }
    else if (hm->count + hm->deleted >= hm->gc_threshold) {
        rehash(hm, hm->alloc);
        return hashmap_put(hm, key, key_len, value);
    }
    else {
        if (kv->key == HM_KEY_DELETED)
            hm->deleted --;
        kv->key = make_key(key, key_len);
        kv->value = value;
        hm->count ++;
        return NULL;
    }
}

void *hashmap_del(HashMap *hm, const void *key, size_t key_len)
{
    void *old_value = NULL;
    struct hm_kv *kv;

    kv = find(hm, key, key_len);

    if (kv && hm_key_valid(kv->key)) {
        free((struct hm_key *) kv->key);
        kv->key = HM_KEY_DELETED;

        old_value = kv->value;
        kv->value = NULL;

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
        struct hm_kv *kv = &hm->kvs[i];

        if (!hm_key_valid(kv->key)) continue;

        r = cb(kv->key->bytes, kv->key->len, kv->value, ctx);
        if (r) return r;
    }

    return 0;
}

extern inline double hashmap_load_factor(const HashMap *hm);
