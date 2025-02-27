#include "flrl/wordlist.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct wordlist wordlist = {0};

int wordlist_init(const char *fname)
{
    FILE *file;
    size_t i, n_words;
    char buf[1024];

    if (!fname) fname = getenv("WORDLIST");
    if (!fname) fname = "/usr/share/dict/words";

    file = fopen(fname, "r");
    if (!file) {
        perror(fname);
        return -1;
    }

    n_words = 0;
    while (fgets(buf, sizeof(buf), file)) {
        n_words ++;
    }

    fseek(file, 0, SEEK_SET);

    wordlist.words = calloc(n_words, sizeof(wordlist.words[0]));
    if (!wordlist.words) {
        perror("calloc");
        return -1;
    }

    i = 0;
    while (i < n_words && fgets(buf, sizeof(buf), file)) {
        char *p;

        p = strpbrk(buf, "\r\n");
        if (p) *p = '\0';

        wordlist.words[i] = strdup(buf);
        if (!wordlist.words[i]) {
            perror("strdup");
            break;
        }

        i++;
    }

    wordlist.n_words = i;
    fclose(file);
    return 0;
}

void wordlist_fini(void)
{
    unsigned i;

    for (i = 0; i < wordlist.n_words; i++)
        free((char *) wordlist.words[i]);

    free(wordlist.words);
    memset(&wordlist, 0, sizeof(wordlist));
}
