#include "flrl/hashmap.h"

#include "flrl/fputil.h"
#include "flrl/randutil.h"
#include "flrl/statsutil.h"

#include <assert.h>
#include <inttypes.h>
#include <stdalign.h>
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
#define HASHMAP_BUCKET_DELETED      UINT16_MAX
#define HASHMAP_INLINE_KEYLEN       (10)

static_assert(1 == __builtin_popcount(HASHMAP_MIN_SIZE));
static_assert(1 == __builtin_popcount(HASHMAP_MAX_SIZE));

struct hm_key {
    union {
        struct {
            void *kptr;
            uint16_t pad__;
        } __attribute__((packed));
        uint8_t kval[HASHMAP_INLINE_KEYLEN];
    };
    uint16_t len;
    uint32_t hash;
} __attribute__((aligned));
static_assert(16 == sizeof(struct hm_key));
static_assert(8 <= alignof(struct hm_key));
static_assert(0 == offsetof(struct hm_key, kval));
static_assert(0 == offsetof(struct hm_key, kptr));
static_assert(10 == offsetof(struct hm_key, len));
static_assert(12 == offsetof(struct hm_key, hash));
#define HM_KEY(hm, i) ((hm)->key[i].len <= HASHMAP_INLINE_KEYLEN    \
                       ? (hm)->key[i].kval                          \
                       : (hm)->key[i].kptr)

#define HM_PSL(hm, i) (((hm)->alloc + (i)                           \
                        - ((hm)->key[i].hash & ((hm)->alloc - 1)))  \
                       & ((hm)->alloc - 1))

#define SWAP(pa, pb) do {   \
    __auto_type _t = *(pa); \
    *(pa) = *(pb);          \
    *(pb) = _t;             \
} while(0)

static uint32_t next_seed = 1;

__attribute__((const))
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

__attribute__((pure))
static inline bool has_key_at_index(const HashMap *hm, uint32_t index)
{
    return hm->key[index].len != HASHMAP_BUCKET_EMPTY
           && hm->key[index].len != HASHMAP_BUCKET_DELETED;
}

__attribute__((pure))
static inline bool should_grow(const HashMap *hm, uint32_t count)
{
    return hm->alloc < HASHMAP_MAX_SIZE
           && hm->grow_threshold != HASHMAP_NO_GROW
           && count >= hm->grow_threshold;
}

__attribute__((pure))
static inline bool should_shrink(const HashMap *hm, uint32_t count)
{
    return hm->alloc > HASHMAP_MIN_SIZE
           && hm->shrink_threshold != HASHMAP_NO_SHRINK
           && count < hm->shrink_threshold;
}

__attribute__((pure))
static inline bool should_gc(const HashMap *hm,
                             uint32_t count, uint32_t deleted)
{
    return hm->gc_threshold != HASHMAP_NO_GC
           && deleted > 0
           && count + deleted >= hm->gc_threshold;
}

static inline int hm_key_init(struct hm_key *hm_key,
                              uint32_t hash, const void *key, size_t key_len)
{
    assert(key_len != HASHMAP_BUCKET_EMPTY);
    assert(key_len != HASHMAP_BUCKET_DELETED);

    if (key_len > HASHMAP_INLINE_KEYLEN) {
        hm_key->kptr = memndup(key, key_len);
        if (!hm_key->kptr)
            return HASHMAP_E_NOMEM;
    }
    else {
        memcpy(hm_key->kval, key, key_len);
    }
    hm_key->len = key_len;
    hm_key->hash = hash;

    return HASHMAP_OK;
}

static int find(const HashMap *hm,
                uint32_t known_hash,
                const void *key, size_t key_len,
                uint32_t *phash,
                uint32_t *pindex)
{
    uint32_t dist, h, i, deleted, mask;
    bool found_deleted = false;

    if (!key || !key_len)
        return HASHMAP_E_INVALID;
    if (key_len > HASHMAP_KEY_MAXLEN)
        return HASHMAP_E_KEYTOOBIG;

    h = known_hash ? known_hash : hashmap_hash32(key, key_len, hm->seed);
    if (phash) *phash = h;

    mask = hm->alloc - 1;
    i = h & mask;
    dist = 0;
    while (dist < hm->alloc) {
        if (hm->key[i].len == HASHMAP_BUCKET_EMPTY
            || dist > HM_PSL(hm, i))
        {
            *pindex = found_deleted ? deleted : i;
            return HASHMAP_E_NOKEY;
        }
        else if (dist == HM_PSL(hm, i)
                 && hm->key[i].len == HASHMAP_BUCKET_DELETED
                 && !found_deleted)
        {
            deleted = i;
            found_deleted = true;
        }
        else if (hm->key[i].len == key_len
                 && hm->key[i].hash == h
                 && 0 == memcmp(HM_KEY(hm, i), key, key_len))
        {
            *pindex = i;
            return HASHMAP_OK;
        }

        i = (i + 1) & mask;
        dist++;
    }

    /* map is full, we ought to have rehashed earlier! */
    return HASHMAP_E_REHASH;
}

static int insert_robinhood(HashMap *hm, uint32_t hash, uint32_t pos,
                            const struct hm_key *key,
                            void *value)
{
    struct hm_key new_key = *key;
    void *new_value = value;
    uint32_t mask = hm->alloc - 1;
    uint32_t dist, i;

    assert(hm->count <= hm->alloc);
    if (hm->count >= hm->alloc)
        return HASHMAP_E_REHASH;
    
    /* XXX */
    assert(hash == new_key.hash);

    /* this key had better not already exist! */
//     assert(!(hm->key[pos].hash == hash
//              && hm->key[pos].len == key.len
//              && memcmp(HM_KEY(hm, pos), key, key_len) == 0));

    i = pos;
    dist = (hm->alloc + i - (hash & mask)) & mask;
    while (has_key_at_index(hm, i)) {
        if (dist > HM_PSL(hm, i)) {
            dist = HM_PSL(hm, i);
            SWAP(&new_key, &hm->key[i]);
            SWAP(&new_value, &hm->value[i]);
        }

        i = (i + 1) & mask;
        dist++;
    }

    if (hm->key[i].len == HASHMAP_BUCKET_DELETED)
        hm->deleted --;
    SWAP(&new_key, &hm->key[i]);
    SWAP(&new_value, &hm->value[i]);

    hm->count ++;
    return HASHMAP_OK;
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
        uint32_t hash, new_i;

        if (!has_key_at_index(hm, i))
            continue;

        r = find(&new_hm, hm->key[i].hash,
                 HM_KEY(hm, i), hm->key[i].len,
                 &hash, &new_i);
        assert(r == HASHMAP_E_NOKEY); /* not found, but got a spot for it */
        assert(new_i < new_hm.alloc);

        /* steal the internals */
        r = insert_robinhood(&new_hm, hash, new_i, &hm->key[i], hm->value[i]);
        assert(r == HASHMAP_OK);
    }

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

    hm->key = calloc(size, sizeof(hm->key[0]));
    hm->value = calloc(size, sizeof(hm->value[0]));

    if (!hm->key || !hm->value) {
        free(hm->key);
        free(hm->value);
        memset(hm, 0, sizeof(*hm));
        return HASHMAP_E_NOMEM;
    }

    hm->alloc = size;
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
            if (hm->key[i].len > HASHMAP_INLINE_KEYLEN)
                free(hm->key[i].kptr);

            if (value_destructor)
                value_destructor(hm->value[i]);
        }
    }

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

    switch (r) {
    case HASHMAP_OK:
        if (value) *value = hm->value[i];
        return HASHMAP_OK;
    case HASHMAP_E_REHASH:
        r = HASHMAP_E_NOKEY;
        /* fall through */
    default:
        if (value) *value= NULL;
        return r;
    }
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
    else if (hm->key[i].len == HASHMAP_BUCKET_EMPTY
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
        struct hm_key new_key = {0};

        r = hm_key_init(&new_key, h, key, key_len);
        if (r) return r;

        if (old_value) *old_value = NULL;
        return insert_robinhood(hm, h, i, &new_key, new_value);
    }
}

int hashmap_del(HashMap *hm, const void *key, size_t key_len, void **old_value)
{
    uint32_t i;
    int r;

    r = find(hm, 0, key, key_len, NULL, &i);

    if (r == HASHMAP_OK) {
        if (old_value) *old_value = hm->value[i];

        if (hm->key[i].len > HASHMAP_INLINE_KEYLEN)
            free(hm->key[i].kptr);

        /* n.b. leave hash in place for psl computations */
        hm->key[i].len = HASHMAP_BUCKET_DELETED;
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
        if (r == HASHMAP_E_REHASH) r = HASHMAP_E_NOKEY;
    }

    return r;
}

int hashmap_foreach(const HashMap *hm, hashmap_foreach_cb *cb, void *ctx)
{
    uint32_t i;
    int r;

    for (i = 0; i < hm->alloc; i++) {
        if (!has_key_at_index(hm, i)) continue;

        r = cb(hm, HM_KEY(hm, i), hm->key[i].len, hm->value[i], ctx);
        if (r) return r;
    }

    return 0;
}

void hashmap_get_stats(const HashMap *hm, HashMapStats *hs)
{
    uint32_t *psl, *bucket_desired_count;
    uint32_t i, n_psl;

    bucket_desired_count = calloc(hm->alloc, sizeof(bucket_desired_count[0]));
    psl = calloc(hm->alloc, sizeof(psl[0]));

    if (!bucket_desired_count || !psl) {
        memset(hs, 0, sizeof(*hs));
        free(bucket_desired_count);
        free(psl);
        return;
    }

    for (i = 0, n_psl = 0; i < hm->alloc; i++) {
        uint32_t db;

        if (!has_key_at_index(hm, i)) continue;

        db = hm->key[i].hash & (hm->alloc - 1);
        bucket_desired_count[db] ++;

        psl[n_psl++] = HM_PSL(hm, i);
    }
    assert(n_psl == hm->count);

    hs->load = 1.0 * hm->count / hm->alloc;

    statsu32v(psl, hm->alloc,
              &hs->psl.min, &hs->psl.min_frequency,
              &hs->psl.max, &hs->psl.max_frequency,
              &hs->psl.mean, &hs->psl.variance);
    hs->psl.median = medianu32v(psl, hm->alloc);
    hs->psl.mode = modeu32v(psl, n_psl, &hs->psl.mode_frequency);

    statsu32v(bucket_desired_count, hm->alloc,
              &hs->bdc.min, &hs->bdc.min_frequency,
              &hs->bdc.max, &hs->bdc.max_frequency,
              &hs->bdc.mean, &hs->bdc.variance);
    hs->bdc.median = medianu32v(bucket_desired_count, hm->alloc);
    hs->bdc.mode = modeu32v(bucket_desired_count, hm->alloc, &hs->bdc.mode_frequency);

    free(bucket_desired_count);
    free(psl);
}

int hashmap_random(const HashMap *hm, struct randbs *rbs,
                   void *key, size_t key_len, void **value)
{
    uint32_t i;

    if (!hm->count) return HASHMAP_E_NOKEY;

    /* XXX this is uniform, but might have perverse runtimes if count is low */
    do {
        i = randu32(rbs, 0, hm->alloc - 1);
    } while (!has_key_at_index(hm, i));

    if (hm->key[i].len != key_len) return HASHMAP_E_INVALID;

    memcpy(key, HM_KEY(hm, i), key_len);
    if (value) *value = hm->value[i];

    return HASHMAP_OK;
}

extern inline uint32_t hashmap_hash32(const void *key, size_t key_len,
                                      uint32_t seed);
