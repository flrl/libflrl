#include "flrl/statsutil.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((unused))
static int usage(void)
{
    fputs("boxplot [options]\n", stderr);
    return 64; /* EX_USAGE */
}

int main(int argc, char **argv)
{
    char line[1024];
    double *data;
    size_t alloc = 128, count = 0;

    setlocale(LC_ALL, ".utf8");

    (void) argc;
    (void) argv;

    data = malloc(alloc * sizeof(data[0]));
    if (!data) return 71; /* EX_OSERR */

    while (fgets(line, sizeof(line), stdin)) {
        char *endptr = NULL;
        double val;

        if (count == alloc) {
            double *tmp;
            if ((tmp = realloc(data, 2 * alloc * sizeof(data[0])))) {
                alloc *= 2;
                data = tmp;
            }
            else {
                perror("realloc");
                break;
            }
        }

        errno = 0;
        val = strtod(line, &endptr);
        if (0 == errno)
            data[count++] = val;
    }

    if (count > 1) {
        struct summary7 s7;

        summary7f64v(&s7, data, count, FENCE_PERC2);

        boxplot_print(NULL,
                      (struct boxplot[]){ { .label = "stdin",
                                            .n_samples = count,
                                            .summary7 = s7 } },
                      1,
                      NULL,
                      stdout);
    }

    free(data);
    return 0;
}
