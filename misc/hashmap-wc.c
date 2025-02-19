#include "flrl/hashmap.h"

#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>


static void incr(HashMap *hm, const char *word, size_t word_len)
{
    uintptr_t v;

    v = (uintptr_t) hashmap_get(hm, word, word_len);
    hashmap_put(hm, word, word_len, (void *) (v + 1), NULL);
}

static int output(const void *key, size_t key_len, void *value, void *ctx)
{
    const char *k = key;
    uintptr_t v = (uintptr_t) value;
    bool print_hash = (bool) ctx;

    if (print_hash) {
        printf("%" PRIu32 " ", hashmap_hash32(key, key_len, 0) % 32768);
    }

    printf("%.*s:\t%" PRIuPTR "\n", (int) key_len, k, v);

    return 0;
}

static void hashmap_wc(const char *fname, int fd, bool print_hash)
{
    HashMap hm;
    char keybuf[256];
    char readbuf[4096];
    size_t key_len = 0;
    ssize_t bytes_read;

    hashmap_init(&hm, 0);

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

    printf("%s:\n", fname);
    hashmap_foreach(&hm, &output, (void *) print_hash);
    hashmap_fini(&hm, NULL);
}

int main(int argc, char **argv)
{
    bool print_hash = false;
    int opt;

    while (-1 != (opt = getopt(argc, argv, "p"))) {
        switch (opt) {
        case 'p':
            print_hash = true;
            break;
        default:
            break;
        }
    }

    if (argc == optind) {
        hashmap_wc("[stdin]", STDIN_FILENO, print_hash);
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

            hashmap_wc(argv[i], fd, print_hash);
            close(fd);
        }
    }

    return 0;
}
