#include "flrl/statsutil.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

const double statsutil_nan = NAN;

void *statsutil_malloc(size_t size)
{
    return malloc(size);
}

void statsutil_free(void *ptr)
{
    free(ptr);
}

double statsutil_round(double x)
{
    return round(x);
}

static inline void ansi_colour_256(FILE *out, uint8_t colour)
{
    fprintf(out, "\e[38;5;%dm", colour);
}

static inline void ansi_reset(FILE *out)
{
    fputs("\e[0m", out);
}

void hist_print(const struct hist_bucket *buckets, size_t n_buckets, FILE *out)
{
    size_t i;
    unsigned p;
    uint8_t bucket_colours[2] = { 242, 249 };
    uint8_t grid_colour = 236;

    for (i = 0; i < n_buckets; i++) {
        int ws;

        if (!buckets[i].freq_raw && buckets[i].skip_if_zero)
            continue;

        /* first line: lower bound label */
        ansi_colour_256(out, bucket_colours[i & 1]);
        assert(strlen(buckets[i].lb_label) <= 9);
        ws = 9 - strlen(buckets[i].lb_label);
        fputs(buckets[i].lb_label, out);
        for (p = 0; (int) p < ws; p++)
            fputc(' ', out);

        /* start grid */
        ansi_colour_256(out, grid_colour);
        fputwc(L'│', out);

        /* pips */
        ansi_colour_256(out, bucket_colours[i & 1]);
        assert(buckets[i].pips <= 60);
        for (p = 0; p < buckets[i].pips; p++)
            fputwc(L'▄', out);

        /* end grid */
        ansi_colour_256(out, grid_colour);
        for (; p < 60; p++) {
            switch ((p + 1) % 6) {
            case 0:
                fputwc(L'│', out);
                break;
            default:
                fputc(' ', out);
                break;
            }
        }

        /* raw frequency */
        ansi_colour_256(out, bucket_colours[i & 1]);
        fprintf(out, " %-8g", (double) buckets[i].freq_raw);
        ansi_reset(out);
        fputc('\n', out);

        /* second line: upper bound label */
        ansi_colour_256(out, bucket_colours[i & 1]);
        assert(strlen(buckets[i].ub_label) <= 9);
        ws = 9 - strlen(buckets[i].ub_label);
        for (p = 0; (int) p < ws; p++)
            fputc(' ', out);
        fputs(buckets[i].ub_label, out);

        /* start grid */
        ansi_colour_256(out, grid_colour);
        fputwc(L'│', out);

        /* pips */
        ansi_colour_256(out, bucket_colours[i & 1]);
        assert(buckets[i].pips <= 60);
        for (p = 0; p < buckets[i].pips; p++)
            fputwc(L'▀', out);

        /* end grid */
        ansi_colour_256(out, grid_colour);
        for (; p < 60; p++) {
            switch ((p + 1) % 6) {
            case 0:
                fputwc(L'│', out);
                break;
            default:
                fputc(' ', out);
                break;
            }
        }

        /* percentage frequency */
        ansi_colour_256(out, bucket_colours[i & 1]);
        fprintf(out, " %8.3g%%", buckets[i].freq_pc);
        ansi_reset(out);
        fputc('\n', out);
    }
    fflush(out);
}
