#ifndef FLRL_WORDLIST_H

#include <stdlib.h>

struct wordlist {
    const char **words;
    size_t n_words;
};

extern struct wordlist wordlist;

extern int wordlist_init(const char *fname);
extern void wordlist_fini(void);
#endif
