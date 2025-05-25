#include "flrl/hashmap.h"
#include "flrl/perf.h"
#include "flrl/randutil.h"
#include "flrl/statsutil.h"
#include "flrl/xoshiro.h"

#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const unsigned lf_n_ops = 100000000;

static bool want_csv = false;
static bool want_graph = false;
static bool want_perf = false;
static bool want_summary = false;

static int usage(void)
{
    fputs("Usage: hashmap-stress [options]\n", stderr);
    return 64; /* XXX EX_USAGE -- mingw doesn't have sysexits.h */
}

typedef void (keygen_fn)(struct randbs *, void **, size_t *);

static void keygen_u32_rand(struct randbs *rbs, void **pkey, size_t *plen)
{
    static uint32_t key;

    key = randu32(rbs, 0, UINT32_MAX);
    *pkey = &key;
    *plen = sizeof(key);
}

static void keygen_u32_seq(struct randbs *rbs __attribute__((unused)),
                           void **pkey, size_t *plen)
{
    static uint32_t next_key = 0;

    *pkey = &next_key;
    *plen = sizeof(next_key);
    next_key ++;
}

static void keygen_vp16_rand(struct randbs *rbs, void **pkey, size_t *plen)
{
    static char word[16];
    unsigned len;

    len = randu32(rbs, 6, sizeof(word));
    randi8v(rbs, (int8_t *) word, len, ' ', '~');

    *pkey = word;
    *plen = len;
}

enum keygen_id {
    KEYGEN_U32_RAND = 0,
    KEYGEN_U32_SEQ,
    KEYGEN_VP16_RAND,

    N_KEYGENS,
};

static const struct keygen {
    const char *name;
    size_t buf_size;
    keygen_fn *keygen;
} keygens[] = {
    { "u32r", sizeof(uint32_t), &keygen_u32_rand },
    { "u32s", sizeof(uint32_t), &keygen_u32_seq },
    { "vp16r", 16, &keygen_vp16_rand },
};
static_assert(N_KEYGENS == sizeof(keygens) / sizeof(keygens[0]));
static const struct keygen *keygen = &keygens[KEYGEN_U32_RAND];

static void do_summary(const HashMap *hm, const char *title)
{
    HashMapStats stats = {0};

    hashmap_get_stats(hm, &stats);

    printf("%" PRIu32 " + %" PRIu32 " / %" PRIu32 " buckets in use\n",
           hm->count, hm->deleted, hm->alloc);
    printf("load factor: %g%% (%g%%)\n", 100.0 * stats.load,
                                         100.0 * (hm->count + hm->deleted) / hm->alloc);

    if (hm->alloc) {
        char bp_title[80];
        const struct boxplot boxplots[] = {
            { .label = "probe sequence length",
              .n_samples = stats.psl.n_samples,
              .summary7 = stats.psl.summary7 },
            { .label = "bucket desired count",
              .n_samples = stats.bdc.n_samples,
              .summary7 = stats.bdc.summary7 },
            { .label = "key length",
              .n_samples = stats.keylen.n_samples,
              .summary7 = stats.keylen.summary7 },
        };
        const size_t n_boxplots = sizeof(boxplots) / sizeof(boxplots[0]);

        snprintf(bp_title, sizeof(bp_title),
                "%s%sload factor %g%% (%g%%)",
                title ? title : "",
                title ? " ": "",
                100.0 * stats.load,
                100.0 * (hm->count + hm->deleted) / hm->alloc);

        boxplot_print(bp_title, boxplots, n_boxplots, NULL, stdout);
    }

    printf("psl mean: %g\n", stats.psl.mean);
    printf("psl stddev: %g\n", sqrt(stats.psl.variance));
    printf("bdc mean: %g\n", stats.bdc.mean);
    printf("bdc stddev: %g\n", sqrt(stats.bdc.variance));
}

static int do_one_load_factor(struct randbs *rbs, double load_factor,
                              struct perf *perf_put,
                              struct perf *perf_del,
                              struct perf *perf_get_existing,
                              struct perf *perf_get_random,
                              struct perf *perf_random)
{
    HashMap hm;
    const unsigned size = 524288; /* 8MB key array, greater than L3 cache */
    void *key = NULL;
    size_t key_len = 0;
    unsigned i;
    int r = 0;

    assert(load_factor > 0.0);
    assert(load_factor < 1.0);

    hashmap_init(&hm, size);
    hm.grow_threshold = HASHMAP_NO_GROW;
    hm.shrink_threshold = HASHMAP_NO_SHRINK;
    hm.gc_threshold = HASHMAP_NO_GC;

// typedef void (keygen_fn)(struct randbs *, void **, size_t *);
    /* fill up to load factor */
    for (i = 0; i < load_factor * size; i++) {
        keygen->keygen(rbs, &key, &key_len);
        r = hashmap_put(&hm, key, key_len, (void *)(uintptr_t) i, NULL);
        if (r) goto done;
    }

    /* alternating random operations */
    for (i = 0; i < lf_n_ops; i++) {
        switch ((i & 3)) {
        case 0:
            /* insert a random new (probably) key */
            keygen->keygen(rbs, &key, &key_len);

            perf_start(perf_put);
            r = hashmap_put(&hm, key, key_len, (void *)(uintptr_t) i, NULL);
            perf_end(perf_put);

            if (r) goto done;
            break;
        case 1:
            /* delete a random existing key */
            perf_start(perf_random);
            r = hashmap_random(&hm, rbs, &key, &key_len, NULL);
            perf_end(perf_random);
            if (r) goto done;

            perf_start(perf_del);
            r = hashmap_del(&hm, key, key_len, NULL);
            perf_end(perf_del);

            free(key);
            if (r) goto done;
            break;
        case 2:
            /* get a random existing key */
            perf_start(perf_random);
            r = hashmap_random(&hm, rbs, &key, &key_len, NULL);
            perf_end(perf_random);
            if (r) goto done;

            perf_start(perf_get_existing);
            r = hashmap_get(&hm, key, key_len, NULL);
            perf_end(perf_get_existing);

            free(key);
            if (r) goto done;
            break;
        case 3:
            /* get a random probably nonexistent key */
            keygen->keygen(rbs, &key, &key_len);

            perf_start(perf_get_random);
            r = hashmap_get(&hm, key, key_len, NULL);
            perf_end(perf_get_random);

            r = 0;
            break;
        }
    }

done:
    if (want_summary)
        do_summary(&hm, NULL);
    hashmap_fini(&hm, NULL);

    return r;
}

static int do_load_factors(struct randbs *rbs,
                           char *load_factor_string,
                           int group_by)
{
    int8_t load_factors[32] = { 0 }; // XXX dynamic...
    char buf[64];
    char *token;
    struct perf **perf = NULL;
    int i = 0, r = 0, n_load_factors;

    for (token = strtok(load_factor_string, " ,");
         token && i < 32;
         token = strtok(NULL, " ,"))
    {
        load_factors[i] = atoi(token);
        if (load_factors[i] <= 0 || load_factors[i] >= 100) {
            fputs("load factors must be within 1-99\n", stderr);
            r = 1;
        }
        i++;
    }

    if (r) return usage();

    n_load_factors = strlen((char*) load_factors);

    if (want_perf) {
        perf = calloc(n_load_factors * 5, sizeof(perf[0]));
        if (!perf) return 71; /* EX_OSERR */

        if (!group_by)
            group_by = n_load_factors == 1 ? 'l' : 'f';
    }

    for (i = 0; !r && i < n_load_factors; i++) {
        if (want_perf) {
            if (group_by == 'l') {
                perf[i * 5 + 0] = perf_new("hashmap_put",
                                           lf_n_ops / 4);
                perf[i * 5 + 1] = perf_new("hashmap_del (existing key)",
                                           lf_n_ops / 4);
                perf[i * 5 + 2] = perf_new("hashmap_get (existing key)",
                                           lf_n_ops / 4);
                perf[i * 5 + 3] = perf_new("hashmap_get (random key)",
                                           lf_n_ops / 4);
                perf[i * 5 + 4] = perf_new("hashmap_random",
                                           lf_n_ops / 4);

                r = do_one_load_factor(rbs, 0.01 * load_factors[i],
                                       perf[i * 5 + 0],
                                       perf[i * 5 + 1],
                                       perf[i * 5 + 2],
                                       perf[i * 5 + 3],
                                       perf[i * 5 + 4]);
            }
            else {
                snprintf(buf, sizeof(buf), "load factor %d%%", load_factors[i]);
                perf[0 * n_load_factors + i] = perf_new(buf, lf_n_ops / 4);
                perf[1 * n_load_factors + i] = perf_new(buf, lf_n_ops / 4);
                perf[2 * n_load_factors + i] = perf_new(buf, lf_n_ops / 4);
                perf[3 * n_load_factors + i] = perf_new(buf, lf_n_ops / 4);
                perf[4 * n_load_factors + i] = perf_new(buf, lf_n_ops / 4);

                r = do_one_load_factor(rbs, 0.01 * load_factors[i],
                                       perf[0 * n_load_factors + i],
                                       perf[1 * n_load_factors + i],
                                       perf[2 * n_load_factors + i],
                                       perf[3 * n_load_factors + i],
                                       perf[4 * n_load_factors + i]);
            }
        }
        else {
            r = do_one_load_factor(rbs, 0.01 * load_factors[i],
                                   NULL, NULL, NULL, NULL, NULL);
        }

        if (r)
            fprintf(stderr, "do_one_load_factor returned %s\n",
                            hashmap_strerr(r));
    }

    if (want_graph) {
        if (group_by == 'l') {
            for (i = 0; i < n_load_factors; i++) {
                snprintf(buf, sizeof(buf), "load factor %d%%", load_factors[i]);
                perf_report(stderr, buf, perf + i * 5, 5);
            }
        }
        else {
            perf_report(stderr, "hashmap_put",
                        perf + 0 * n_load_factors, n_load_factors);

            perf_report(stderr, "hashmap_del (existing key)",
                        perf + 1 * n_load_factors, n_load_factors);

            perf_report(stderr, "hashmap_get (existing key)",
                        perf + 2 * n_load_factors, n_load_factors);

            perf_report(stderr, "hashmap_get (random key)",
                        perf + 3 * n_load_factors, n_load_factors);

            perf_report(stderr, "hashmap_random",
                        perf + 4 * n_load_factors, n_load_factors);
        }
    }

    if (want_perf) {
        for (i = 0; i < n_load_factors * 5; i++) {
            free(perf[i]);
        }
        free(perf);
    }

    return r;
}

static int do_grow(struct randbs *rbs)
{
    HashMap hm;
    struct perf *perf_put = NULL;
    const uint32_t initial_size = 1000000, target_size = 100000000;
    uintptr_t i = 0;

    hashmap_init(&hm, initial_size);

    if (want_perf)
        perf_put = perf_new("hashmap_put", target_size);

    while (hm.count < target_size) {
        void *key;
        size_t key_len;

        keygen->keygen(rbs, &key, &key_len);

        perf_start(perf_put);
        hashmap_put(&hm, key, key_len, (void *) i, NULL);
        perf_end(perf_put);
    }

    if (want_graph) {
        perf_report(stdout, "growing hash", &perf_put, 1);
    }

    free(perf_put);

    if (want_summary)
        do_summary(&hm, "grow");
    hashmap_fini(&hm, NULL);

    return 0;
}

static int do_shrink(struct randbs *rbs)
{
    HashMap hm;
    struct perf *perf_del = NULL;
    uint8_t *keys = NULL;
    const uint32_t n_keys = 100000000;
    uintptr_t i = 0;
    int r;

    assert(keygen->buf_size <= 255);

    keys = calloc(n_keys, 1 + keygen->buf_size);
    if (!keys) return 71; /* EX_OSERR */

    r = hashmap_init(&hm, n_keys);
    if (r) goto done;

    for (i = 0; i < n_keys; i++) {
        void *key;
        size_t key_len;

        do {
            keygen->keygen(rbs, &key, &key_len);
        } while (HASHMAP_OK == hashmap_get(&hm, key, key_len, NULL));

        keys[i * (1 + keygen->buf_size)] = key_len;
        memcpy(&keys[i * (1 + keygen->buf_size) + 1], key, key_len);

        r = hashmap_put(&hm, key, key_len, (void *) i, NULL);
        if (r) goto done;
    }

    shuffle(rbs, keys, n_keys, 1 + keygen->buf_size);

    if (want_perf)
        perf_del = perf_new("hashmap_del", n_keys);

    for (i = 0; i < n_keys; i++) {
        void *key = &keys[i * (1 + keygen->buf_size) + 1];
        size_t key_len = keys[i * (1 + keygen->buf_size)];
        void *old_value;

        perf_start(perf_del);
        r = hashmap_del(&hm, key, key_len, &old_value);
        perf_end(perf_del);

        if (r) goto done;
    }

    if (want_graph) {
        perf_report(stdout, "shrinking hash", &perf_del, 1);
    }

 done:
    free(keys);

    if (want_summary)
        do_summary(&hm, "shrink");
    hashmap_fini(&hm, NULL);

    return r;
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        { "load-factor-group-by", required_argument, NULL, 'L' },
        { "csv",                  no_argument,       NULL, 'C' },
        { "graph",                no_argument,       NULL, 'G' },
        { "keygen",               required_argument, NULL, 'K' },
        { "summary",              no_argument,       NULL, 'S' },
        { "grow",                 no_argument,       NULL, 'g' },
        { "load-factor",          required_argument, NULL, 'l' },
        { "shrink",               no_argument,       NULL, 's' },
        { NULL,                   0,                 NULL,  0  },
    };
    struct randbs rbs = RANDBS_INITIALIZER(&xoshiro128plusplus_next);
    char *load_factor_string = NULL;
    const char *want_keygen = NULL;
    unsigned i;
    int load_factor_group_by = 0;
    int c, r = 0;
    bool want_grow = false, want_shrink = false;

    setlocale(LC_ALL, ".utf8");
    randbs_seed64(&rbs, UINT64_C(11226047971600110276));

    while (-1 != (c = getopt_long(argc, argv, "L:CGSgl:s", long_options, NULL))) {
        switch (c) {
        case 'L':
            load_factor_group_by = optarg[0];
            if (load_factor_group_by != 'l' && load_factor_group_by != 'f')
                r = usage();
            break;
        case 'C':
            want_csv = true;
            want_perf = true;
            break;
        case 'G':
            want_graph = true;
            want_perf = true;
            break;
        case 'K':
            want_keygen = optarg;
            break;
        case 'S':
            want_summary = true;
            break;
        case 'g':
            want_grow = true;
            break;
        case 'l':
            load_factor_string = strdup(optarg);
            break;
        case 's':
            want_shrink = true;
            break;
        default:
            r = usage();
            break;
        }
    }

    if (want_keygen) {
        const struct keygen *found = NULL;

        for (i = 0; i < N_KEYGENS; i++) {
            if (0 == strcmp(keygens[i].name, want_keygen)) {
                found = &keygens[i];
                break;
            }
        }

        if (found)
            keygen = found;
        else
            r = usage();
    }

    if (!r && load_factor_string) {
        r = do_load_factors(&rbs, load_factor_string, load_factor_group_by);
    }
    free(load_factor_string);

    if (!r && want_grow) {
        r = do_grow(&rbs);
    }

    if (!r && want_shrink) {
        r = do_shrink(&rbs);
    }

    return r;
}
