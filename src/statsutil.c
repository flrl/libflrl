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

void *statsutil_calloc(size_t nelem, size_t elsize)
{
    return calloc(nelem, elsize);
}

void statsutil_free(void *ptr)
{
    free(ptr);
}

double statsutil_ceil(double x)
{
    return ceil(x);
}

double statsutil_floor(double x)
{
    return floor(x);
}

double statsutil_niceceil(double x)
{
    double scale = 1.0;

    while (x >= 10.0) {
        x /= 10.0;
        scale *= 10.0;
    }

    while (x < 1.0) {
        x *= 10.0;
        scale /= 10.0;
    }

    x = 0.5 * ceil(2.0 * x);

    return scale * x;
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

static const uint8_t grid_colour = 236;

void hist_print_header(const char *title, double grid[6], FILE *out)
{
    unsigned i;
    int ws;

    ansi_colour_256(out, grid_colour);
    fputws(L"════════════════════════════════════════"
           L"════════════════════════════════════════",
           out);
    ansi_reset(out);
    fputc('\n', out);

    assert(strlen(title) < 80);
    ws = (80 - strlen(title)) / 2;
    for (i = 0; (int) i < ws; i++)
        fputc(' ', out);
    fputs(title, out);
    fputc('\n', out);

    ansi_colour_256(out, grid_colour);
    fputws(L"════════════════════════════════════════"
           L"════════════════════════════════════════",
           out);
    ansi_reset(out);
    fputs("\n         ", out);
    fprintf(out, "%-7g   ", 0.0);
    for (i = 0; i < 6; i++) {
        fprintf(out, "%-7g   ", grid[i]);
    }
    fputc('\n', out);
    ansi_colour_256(out, grid_colour);
    fputws(L"─────────┼─────────┼─────────┼─────────┼"
           L"─────────┼─────────┼─────────┼──────────",
           out);
    ansi_reset(out);
    fputc('\n', out);
}

void hist_print_footer(FILE *out)
{
    ansi_colour_256(out, grid_colour);
    fputws(L"─────────┴─────────┴─────────┴─────────┴"
           L"─────────┴─────────┴─────────┴──────────",
           out);
    ansi_reset(out);
    fputc('\n', out);
}

void hist_print(const struct hist_bucket *buckets, size_t n_buckets, FILE *out)
{
    size_t i;
    unsigned p;
    uint8_t bucket_colours[2] = { 242, 249 };

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
            switch ((p + 1) % 10) {
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
            switch ((p + 1) % 10) {
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
