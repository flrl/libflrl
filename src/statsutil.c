#include "flrl/statsutil.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

const double statsutil_nan = NAN;

static const uint8_t grid_colour = 236;
static const uint8_t bucket_colours[2] = { 242, 249 };
static const uint8_t bucket_bgcolours[2] = { 0, 232 };

void *statsutil_malloc(size_t size)
{
    return malloc(size);
}

void *statsutil_calloc(size_t nelem, size_t elsize)
{
    return calloc(nelem, elsize);
}

char *statsutil_strdup(const char *s)
{
    return strdup(s);
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

double statsutil_round(double x)
{
    return round(x);
}

static inline void ansi_colour_256(FILE *out, uint8_t colour)
{
    fprintf(out, "\e[38;5;%dm", colour);
}

static inline void ansi_bgcolour_256(FILE *out, uint8_t colour)
{
    fprintf(out, "\e[48;5;%dm", colour);
}

static inline void ansi_reset(FILE *out)
{
    fputs("\e[0m", out);
}

static void hist_print_header(const Histogram *hist, FILE *out)
{
    unsigned i;
    int ws;

    ansi_colour_256(out, grid_colour);
    fputws(L"════════════════════════════════════════"
           L"════════════════════════════════════════",
           out);
    ansi_reset(out);
    fputc('\n', out);

    assert(strlen(hist->title) < 80);
    ws = (80 - strlen(hist->title)) / 2;
    for (i = 0; (int) i < ws; i++)
        fputc(' ', out);
    fputs(hist->title, out);
    fputc('\n', out);

    ansi_colour_256(out, grid_colour);
    fputws(L"════════════════════════════════════════"
           L"════════════════════════════════════════",
           out);
    ansi_reset(out);
    fputs("\n         ", out);
    fprintf(out, "%-7g   ", 0.0);
    for (i = 0; i < 6; i++) {
        fprintf(out, "%-7g   ", hist->grid[i]);
    }
    fputc('\n', out);
    ansi_colour_256(out, grid_colour);
    fputws(L"─────────┼─────────┼─────────┼─────────┼"
           L"─────────┼─────────┼─────────┼──────────",
           out);
    ansi_reset(out);
    fputc('\n', out);
}

static void hist_print_footer(FILE *out)
{
    ansi_colour_256(out, grid_colour);
    fputws(L"─────────┴─────────┴─────────┴─────────┴"
           L"─────────┴─────────┴─────────┴──────────",
           out);
    ansi_reset(out);
    fputc('\n', out);
}

void histogram_print(const Histogram *hist, FILE *out)
{
    size_t i;
    unsigned p;

    hist_print_header(hist, out);

    for (i = 0; i < hist->n_buckets; i++) {
        struct hist_bucket *bucket = &hist->buckets[i];
        int ws;

        if (!bucket->freq_raw && bucket->skip_if_zero)
            continue;

        /* first line: lower bound label */
        ansi_colour_256(out, bucket_colours[i & 1]);
        ansi_bgcolour_256(out, bucket_bgcolours[i & 1]);
        assert(strlen(bucket->lb_label) <= 9);
        ws = 9 - strlen(bucket->lb_label);
        fputs(bucket->lb_label, out);
        for (p = 0; (int) p < ws; p++)
            fputc(' ', out);

        /* start grid */
        ansi_colour_256(out, grid_colour);
        fputwc(L'│', out);

        /* pips */
        ansi_colour_256(out, bucket_colours[i & 1]);
        assert(bucket->pips <= 60);
        for (p = 0; p < bucket->pips; p++)
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
        fprintf(out, " %-9g", (double) bucket->freq_raw);
        ansi_reset(out);
        fputc('\n', out);

        /* second line: upper bound label */
        ansi_colour_256(out, bucket_colours[i & 1]);
        ansi_bgcolour_256(out, bucket_bgcolours[i & 1]);
        assert(strlen(bucket->ub_label) <= 9);
        ws = 9 - strlen(bucket->ub_label);
        for (p = 0; (int) p < ws; p++)
            fputc(' ', out);
        fputs(bucket->ub_label, out);

        /* start grid */
        ansi_colour_256(out, grid_colour);
        fputwc(L'│', out);

        /* pips */
        ansi_colour_256(out, bucket_colours[i & 1]);
        assert(bucket->pips <= 60);
        for (p = 0; p < bucket->pips; p++)
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
        fprintf(out, " %8.3g%%", bucket->freq_pc);
        ansi_reset(out);
        fputc('\n', out);
    }

    hist_print_footer(out);
    fflush(out);
}

void histogram_fini(Histogram *hist)
{
    free(hist->title);
    free(hist->buckets);
    memset(hist, 0, sizeof(*hist));
}
