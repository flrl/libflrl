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
#define HASHMAP_GROW_THRESHOLD      (0.84)
#define HASHMAP_SHRINK_THRESHOLD    (0.30)
#define HASHMAP_BUCKET_EMPTY        UINT16_C(0)
#define HASHMAP_INLINE_KEYLEN       (14)
#define HASHMAP_PAD                 (HASHMAP_INLINE_KEYLEN - sizeof(void*))

static_assert(1 == __builtin_popcount(HASHMAP_MIN_SIZE));
static_assert(1 == __builtin_popcount(HASHMAP_MAX_SIZE));

struct hm_key {
    union {
        struct {
            void *kptr;
            uint8_t pad__[HASHMAP_PAD];
        } __attribute__((packed));
        uint8_t kval[HASHMAP_INLINE_KEYLEN];
    };
    uint8_t len;
    uint8_t psl;
} __attribute__((aligned));
static_assert(16 == sizeof(struct hm_key));
static_assert(8 <= alignof(struct hm_key));
static_assert(0 == offsetof(struct hm_key, kval));
static_assert(0 == offsetof(struct hm_key, kptr));
static_assert(14 == offsetof(struct hm_key, len));
static_assert(15 == offsetof(struct hm_key, psl));
#define HM_KEY(hm, i) ((hm)->key[i].len <= HASHMAP_INLINE_KEYLEN    \
                       ? (hm)->key[i].kval                          \
                       : (hm)->key[i].kptr)

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
    return hm->key[index].len != HASHMAP_BUCKET_EMPTY;
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

static inline int hm_key_init(struct hm_key *hm_key,
                              const void *key, size_t key_len)
{
    assert(key_len != HASHMAP_BUCKET_EMPTY);
    assert(key_len <= HASHMAP_MAX_KEYLEN);

    if (key_len > HASHMAP_INLINE_KEYLEN) {
        hm_key->kptr = memndup(key, key_len);
        if (!hm_key->kptr)
            return HASHMAP_E_NOMEM;
    }
    else {
        memcpy(hm_key->kval, key, key_len);
    }
    hm_key->len = key_len;
    hm_key->psl = 0;

    return HASHMAP_OK;
}

static inline int keycmp4(const struct hm_key *a,
                          const void *b_key, size_t b_len)
{
    if (a->len != b_len) {
        /* smallest len goes first */
        return (a->len > b_len) - (a->len < b_len);
    }
    else if (a->len <= HASHMAP_INLINE_KEYLEN) {
        return memcmp(a->kval, b_key, b_len);
    }
    else {
        return memcmp(a->kptr, b_key, b_len);
    }
}

static inline int keycmp(const struct hm_key *a, const struct hm_key *b)
{
    return keycmp4(a,
                   b->len <= HASHMAP_INLINE_KEYLEN ? b->kval : b->kptr,
                   b->len);
}

static int find(const HashMap *hm,
                uint32_t hash,
                const void *key, size_t key_len,
                uint32_t *pindex)
{
    uint32_t dist, i, pip, mask;
    bool found_pip = false;

    if (!key || !key_len)
        return HASHMAP_E_INVALID;
    if (key_len > HASHMAP_MAX_KEYLEN)
        return HASHMAP_E_KEYTOOBIG;

    mask = hm->alloc - 1;
    i = hash & mask;
    dist = 0;
    while (dist < hm->alloc) {
        if (hm->key[i].len == HASHMAP_BUCKET_EMPTY
            || dist > hm->key[i].psl)
        {
            *pindex = found_pip ? pip : i;
            return HASHMAP_E_NOKEY;
        }
        else if (dist == hm->key[i].psl
                 && !found_pip
                 && keycmp4(&hm->key[i], key, key_len) > 0)
        {
            /* don't yet know if the key exists, but if in the end it doesn't,
             * here's a possible insertion point
             */
            pip = i;
            found_pip = true;
        }
        else if (0 == keycmp4(&hm->key[i], key, key_len)) {
            *pindex = i;
            return HASHMAP_OK;
        }

        i = (i + 1) & mask;
        dist++;
    }

    /* map is full, we ought to have resized earlier! */
    return HASHMAP_E_RESIZE;
}

static int insert_robinhood(HashMap *hm, uint32_t hash, uint32_t pos,
                            const struct hm_key *key, void *value)
{
    struct hm_key new_key = *key;
    void *new_value = value;
    uint32_t mask = hm->alloc - 1;
    uint32_t dist, i;

    assert(hm->count <= hm->alloc);
    if (hm->count >= hm->alloc)
        return HASHMAP_E_RESIZE;
    
    i = pos;
    __builtin_prefetch(&hm->value[i]);
    dist = (hm->alloc + i - (hash & mask)) & mask;
    while (has_key_at_index(hm, i)) {
        uint32_t psl = hm->key[i].psl;

        if (dist > psl
            || (dist == psl && keycmp(&new_key, &hm->key[i]) < 0))
        {
            new_key.psl = dist;
            SWAP(&new_key, &hm->key[i]);
            SWAP(&new_value, &hm->value[i]);
            dist = psl;
        }

        i = (i + 1) & mask;
        dist++;
    }

    new_key.psl = dist;
    SWAP(&new_key, &hm->key[i]);
    SWAP(&new_value, &hm->value[i]);

    hm->count ++;
    return HASHMAP_OK;
}

static int delete_robinhood(HashMap *hm, uint32_t pos, void **old_value)
{
    const uint32_t mask = hm->alloc - 1;
    uint32_t next = (pos + 1) & mask;
    void *freeme = hm->key[pos].len > HASHMAP_INLINE_KEYLEN
                 ? hm->key[pos].kptr
                 : NULL;

    assert(hm->count > 0);
    assert(hm->alloc > 0);
    assert(has_key_at_index(hm, pos));

    __builtin_prefetch(&hm->value[pos]);
    __builtin_prefetch(&hm->key[next]);

    hm->key[pos] = (struct hm_key) {
        .kval = { 0 },
        .len = HASHMAP_BUCKET_EMPTY,
        .psl = 0,
    };
    if (old_value) *old_value = hm->value[pos];
    hm->value[pos] = NULL;
    hm->count --;

    __builtin_prefetch(&hm->value[next]);

    while (has_key_at_index(hm, next)) {
        if (0 == hm->key[next].psl) break;

        SWAP(&hm->key[pos], &hm->key[next]);
        SWAP(&hm->value[pos], &hm->value[next]);

        hm->key[pos].psl --;

        pos = next;
        next = (pos + 1) & mask;
    }

    free(freeme);
    return HASHMAP_OK;
}

int hashmap_resize(HashMap *hm, uint32_t new_size)
{
    HashMap new_hm;
    uint32_t i;
    int r;

    new_size = nextpow2(new_size);

    if (new_size > HASHMAP_MAX_SIZE)
        return HASHMAP_E_INVALID;

    if (new_size < HASHMAP_MIN_SIZE)
        new_size = HASHMAP_MIN_SIZE;

    assert(new_size >= hm->count);
    if (new_size < hm->count)
        return HASHMAP_E_INVALID;

    r = hashmap_init(&new_hm, new_size);
    if (r) return r;

    if (hm->grow_threshold == HASHMAP_NO_GROW)
        new_hm.grow_threshold = HASHMAP_NO_GROW;

    if (hm->shrink_threshold == HASHMAP_NO_SHRINK)
        new_hm.shrink_threshold = HASHMAP_NO_SHRINK;

    /* XXX reuse the existing seed */
    new_hm.seed = hm->seed;
    next_seed --;

    for (i = 0; i < hm->alloc; i++) {
        uint32_t hash, new_i;

        if (!has_key_at_index(hm, i))
            continue;

        hash = hashmap_hash32(HM_KEY(hm, i), hm->key[i].len, new_hm.seed);
        r = find(&new_hm, hash, HM_KEY(hm, i), hm->key[i].len, &new_i);
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
    case HASHMAP_E_RESIZE:
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
    hm->count = 0;
    hm->seed = next_seed ++;

    hm->grow_threshold = size < HASHMAP_MAX_SIZE
                         ? (uint32_t) (size * HASHMAP_GROW_THRESHOLD) - 1
                         : HASHMAP_NO_GROW;
    hm->shrink_threshold = size > HASHMAP_MIN_SIZE
                         ? (uint32_t) (size * HASHMAP_SHRINK_THRESHOLD) - 1
                         : HASHMAP_NO_SHRINK;

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
    uint32_t i, hash;
    int r;

    hash = hashmap_hash32(key, key_len, hm->seed);
    r = find(hm, hash, key, key_len, &i);

    switch (r) {
    case HASHMAP_OK:
        if (value) *value = hm->value[i];
        return HASHMAP_OK;
    case HASHMAP_E_RESIZE:
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
    uint32_t hash, i;
    int r;

    hash = hashmap_hash32(key, key_len, hm->seed);
    r = find(hm, hash, key, key_len, &i);

    if (r == HASHMAP_E_RESIZE) {
        if (hm->grow_threshold == HASHMAP_NO_GROW
            || (r = hashmap_resize(hm, hm->alloc * 2)))
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
    else if (should_grow(hm, hm->count)) {
        hashmap_resize(hm, hm->alloc * 2);
        return hashmap_put(hm, key, key_len, new_value, old_value);
    }
    else {
        struct hm_key new_key = {0};

        r = hm_key_init(&new_key, key, key_len);
        if (r) return r;

        if (old_value) *old_value = NULL;
        return insert_robinhood(hm, hash, i, &new_key, new_value);
    }
}

int hashmap_del(HashMap *hm, const void *key, size_t key_len, void **old_value)
{
    uint32_t i, hash;
    int r;

    hash = hashmap_hash32(key, key_len, hm->seed);
    r = find(hm, hash, key, key_len, &i);

    if (r == HASHMAP_OK) {
        r = delete_robinhood(hm, i, old_value);

        if (should_shrink(hm, hm->count))
            hashmap_resize(hm, hm->alloc / 2);
    }
    else {
        if (old_value) *old_value = NULL;
        if (r == HASHMAP_E_RESIZE) r = HASHMAP_E_NOKEY;
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
    uint16_t *keylen;
    uint32_t i, mask = hm->alloc - 1, n_keys;

    bucket_desired_count = calloc(hm->alloc, sizeof(bucket_desired_count[0]));
    psl = calloc(hm->alloc, sizeof(psl[0]));
    keylen = calloc(hm->alloc, sizeof(keylen[0]));

    if (!bucket_desired_count || !psl) {
        memset(hs, 0, sizeof(*hs));
        free(bucket_desired_count);
        free(psl);
        return;
    }

    for (i = 0, n_keys = 0; i < hm->alloc; i++) {
        uint32_t db;

        if (!has_key_at_index(hm, i)) continue;

        db = (i + hm->alloc - hm->key[i].psl) & mask;
        bucket_desired_count[db] ++;

        psl[n_keys] = hm->key[i].psl;
        keylen[n_keys] = hm->key[i].len;
        n_keys ++;
    }
    assert(n_keys == hm->count);

    hs->load = 1.0 * hm->count / hm->alloc;

    summary7u32v(&hs->psl.summary7, psl, n_keys, FENCE_PERC2);
    hs->psl.mean = meanu32v(psl, n_keys);
    hs->psl.variance = varianceu32v(psl, n_keys, hs->psl.mean);
    hs->psl.n_samples = n_keys;

    summary7u32v(&hs->bdc.summary7, bucket_desired_count, hm->alloc, FENCE_PERC2);
    hs->bdc.mean = meanu32v(bucket_desired_count, hm->alloc);
    hs->bdc.variance = varianceu32v(bucket_desired_count, hm->alloc, hs->bdc.mean);
    hs->bdc.n_samples = hm->alloc;

    summary7u16v(&hs->keylen.summary7, keylen, n_keys, FENCE_PERC2);
    hs->keylen.mean = meanu16v(keylen, n_keys);
    hs->keylen.variance = varianceu16v(keylen, n_keys, hs->keylen.mean);
    hs->keylen.n_samples = n_keys;

    free(bucket_desired_count);
    free(psl);
    free(keylen);
}

int hashmap_random(const HashMap *hm, struct randbs *rbs,
                   void **pkey, size_t *pkey_len, void **pvalue)
{
    uint32_t i;

    if (!hm->count) return HASHMAP_E_NOKEY;
    if (!pkey || !pkey_len) return HASHMAP_E_INVALID;

    /* XXX this is uniform, but might have perverse runtimes if count is low */
    do {
        i = randu32(rbs, 0, hm->alloc - 1);
    } while (!has_key_at_index(hm, i));

    *pkey = memndup(HM_KEY(hm, i), hm->key[i].len);
    *pkey_len = hm->key[i].len;
    if (pvalue) *pvalue = hm->value[i];

    return HASHMAP_OK;
}

extern inline uint32_t hashmap_hash32(const void *key, size_t key_len,
                                      uint32_t seed);
