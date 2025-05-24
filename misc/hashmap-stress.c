#include "flrl/hashmap.h"
#include "flrl/perf.h"
#include "flrl/randutil.h"
#include "flrl/statsutil.h"
#include "flrl/xoshiro.h"

#include <assert.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const unsigned lf_n_ops = 100000000;

static bool want_csv = false;
static bool want_graph = false;
static bool want_perf = false;

static int usage(void)
{
    fputs("Usage: hashmap-stress [options]\n", stderr);
    return 64; /* XXX EX_USAGE -- mingw doesn't have sysexits.h */
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
    unsigned i;
    int r = 0;

    assert(load_factor > 0.0);
    assert(load_factor < 1.0);

    hashmap_init(&hm, size);
    hm.grow_threshold = HASHMAP_NO_GROW;
    hm.shrink_threshold = HASHMAP_NO_SHRINK;
    hm.gc_threshold = HASHMAP_NO_GC;

    /* fill up to load factor */
    for (i = 0; i < load_factor * size; i++) {
        uint32_t key = randu32(rbs, 0, UINT32_MAX);
        r = hashmap_put(&hm, &key, sizeof(key), (void *)(uintptr_t) i, NULL);
        if (r) goto done;
    }

    /* alternating random operations */
    for (i = 0; i < lf_n_ops; i++) {
        void *key;
        size_t key_len;
        uint32_t x;

        switch ((i & 3)) {
        case 0:
            /* insert a random new (probably) key */
            x = randu32(rbs, 0, UINT32_MAX);
            key = &x;
            key_len = sizeof(x);

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
            x = randu32(rbs, 0, UINT32_MAX);
            key = &x;
            key_len = sizeof(x);

            perf_start(perf_get_random);
            r = hashmap_get(&hm, key, key_len, NULL);
            perf_end(perf_get_random);

            r = 0;
            break;
        }
    }

done:
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

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        { "load-factor-group-by", required_argument, NULL, 'L' },
        { "csv",                  no_argument,       NULL, 'c' },
        { "graph",                no_argument,       NULL, 'g' },
        { "load-factor",          required_argument, NULL, 'l' },
        { NULL,                   0,                 NULL,  0  },
    };
    int c, r = 0;
    struct randbs rbs = RANDBS_INITIALIZER(&xoshiro128plusplus_next);
    char *load_factor_string = NULL;
    int load_factor_group_by = 0;

    setlocale(LC_ALL, ".utf8");
    randbs_seed64(&rbs, time(NULL));

    while (-1 != (c = getopt_long(argc, argv, "L:cgl:", long_options, NULL))) {
        switch (c) {
        case 'L':
            load_factor_group_by = optarg[0];
            if (load_factor_group_by != 'l' && load_factor_group_by != 'f')
                r = usage();
            break;
        case 'c':
            want_csv = true;
            want_perf = true;
            break;
        case 'g':
            want_graph = true;
            want_perf = true;
            break;
        case 'l':
            load_factor_string = strdup(optarg);
            break;
        default:
            r = usage();
            break;
        }
    }

    if (!r && load_factor_string) {
        r = do_load_factors(&rbs, load_factor_string, load_factor_group_by);
    }
    free(load_factor_string);

    return r;
}
