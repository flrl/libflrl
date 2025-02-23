#include "flrl/hashmap.h"

#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static struct {
    bool print_hash;
    uint32_t hash_mask;
    bool set_seed;
    uint32_t seed;
    bool dump_psl;
    bool quiet;
} options = {
    .print_hash = false,
    .hash_mask = UINT32_MAX,
    .set_seed = false,
    .seed = UINT32_C(0),
    .dump_psl = false,
    .quiet = false,
};

static void incr(HashMap *hm, const char *word, size_t word_len)
{
    uintptr_t v = 0;

    hashmap_get(hm, word, word_len, (void **) &v);
    hashmap_put(hm, word, word_len, (void *) (v + 1), NULL);
}

static int output(const HashMap *hm,
                  const void *key,
                  size_t key_len,
                  void *value,
                  void *ctx __attribute__((unused)))
{
    const char *k = key;
    uintptr_t v = (uintptr_t) value;

    if (options.print_hash) {
        printf("%" PRIx32 " ",
               hashmap_hash32(key, key_len, hm->seed) & options.hash_mask);
    }

    printf("%.*s:\t%" PRIuPTR "\n", (int) key_len, k, v);

    return 0;
}

static void hashmap_wc(const char *fname, int fd)
{
    HashMap hm;
    char keybuf[256];
    char readbuf[4096];
    size_t key_len = 0;
    ssize_t bytes_read;

    hashmap_init(&hm, 0);
    if (options.set_seed)
        hm.seed = options.seed;

    while (0 < (bytes_read = read(fd, readbuf, sizeof(readbuf)))) {
        ssize_t i;

        for (i = 0; i < bytes_read; i++) {
            int c = readbuf[i];

            if ((isalnum(c) || c == '_') && key_len < HASHMAP_KEY_MAXLEN) {
                keybuf[key_len++] = c;
            }
            else if (key_len) {
                incr(&hm, keybuf, key_len);
                memset(keybuf, 0, sizeof(keybuf));
                key_len = 0;
            }
        }
    }
    /* last line might not have had eol */
    if (key_len) {
        incr(&hm, keybuf, key_len);
        memset(keybuf, 0, sizeof(keybuf));
        key_len = 0;
    }

    if (!options.quiet) {
        printf("%s:\n", fname);
        hashmap_foreach(&hm, &output, NULL);
    }

    if (options.dump_psl) {
        HashMapStats stats;

        hashmap_get_stats(&hm, &stats);

        printf("%" PRIu32 "/%" PRIu32 " buckets in use\n",
               hm.count, hm.alloc);
        printf("load factor: %g\n", stats.load);
        printf("min psl: %" PRIu32 "\n", stats.psl.min);
        printf("max psl: %" PRIu32 "\n", stats.psl.max);
        printf("avg psl: %g variance: %g stddev: %g\n",
               stats.psl.avg, stats.psl.var, sqrt(stats.psl.var));
    }

    hashmap_fini(&hm, NULL);
}

int main(int argc, char **argv)
{
    int opt;

    while (-1 != (opt = getopt(argc, argv, "hm:pqs:"))) {
        switch (opt) {
        case 'h':
            options.print_hash = true;
            break;
        case 'm':
            errno = 0;
            options.hash_mask = strtoul(optarg, NULL, 0);
            if (errno) options.hash_mask = 0;
            break;
        case 'p':
            options.dump_psl = true;
            break;
        case 'q':
            options.quiet = true;
            break;
        case 's':
            errno = 0;
            options.seed = strtoul(optarg, NULL, 0);
            if (errno) options.seed = 0;
            options.set_seed = true;
        default:
            break;
        }
    }

    if (argc == optind) {
        hashmap_wc("[stdin]", STDIN_FILENO);
    }
    else {
        int i;

        for (i = optind; i < argc; i++) {
            int fd;

            fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                perror(argv[i]);
                continue;
            }

            hashmap_wc(argv[i], fd);
            close(fd);
        }
    }

    return 0;
}
