#include "flrl/statsutil.h"

#include "flrl/fputil.h"

#include <assert.h>
#include <inttypes.h>
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

static const wchar_t *default_format_sample_cb(wchar_t buf[11], double sample)
{
    int f;

    f = swprintf(buf, 11, L"%.4g", sample);
    assert(f > 0 && f < 11);
    return buf;
}

struct boxplot_meta {
    wchar_t show[7];
    int pos[7];
};

static void boxplot_print_header(const char *title,
                                 double grid_lines[7],
                                 boxplot_format_sample_cb *formatter,
                                 FILE *out)
{
    int title_len = strlen(title);
    int i;

    ansi_colour_256(out, grid_colour);
    fputws(L"════════════════════════════════════════"
           L"════════════════════════════════════════\n",
           out);
    ansi_reset(out);

    if (title_len > 78) title_len = 78;
    fprintf(out, "%*.*s\n",
                 title_len + (80 - title_len) / 2,
                 title_len,
                 title);

    ansi_colour_256(out, grid_colour);
    fputws(L"════════════════════════════════════════"
           L"════════════════════════════════════════\n",
           out);
    ansi_reset(out);

    fputs("   ", out);
    for (i = 0; i < 7; i++) {
        wchar_t grid_label[11];

        fprintf(out, "%11.10ls", formatter(grid_label, grid_lines[i]));
    }
    fputs("\n", out);

    ansi_colour_256(out, grid_colour);
    fputws(L"─────────────┼──────────┼──────────┼────"
           L"──────┼──────────┼──────────┼──────────┤\n",
           out);
    ansi_reset(out);
}

static void boxplot_print_one(const struct boxplot *bp,
                              const struct boxplot_meta *meta,
                              boxplot_format_sample_cb *formatter,
                              enum summary7_fence fence,
                              bool is_odd,
                              FILE *out)
{
    const wchar_t *lower_fence[5] = { L"lno", L"⅛", L"10%", L" 9%", L" 2%" };
    const wchar_t *upper_fence[5] = { L"hno", L"⅞", L"90%", L"91%", L"98%" };
    const int label_len = bp->label ? strlen(bp->label) : 0;
    wchar_t buf[32];
    wchar_t min_label[32], max_label[32];
    int i, r;

//     for (i = 0; i < 7; i++) {
//         fprintf(stderr, "meta[%d]: pos=%d show=%lc\n",
//                         i, meta->pos[i], meta->show[i]);
//     }

    swprintf(min_label, 32, L"min: %ls", formatter(buf, bp->quantiles[0]));
    swprintf(max_label, 32, L"max: %ls", formatter(buf, bp->quantiles[6]));

    // first line
    ansi_bgcolour_256(out, bucket_bgcolours[is_odd]);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fprintf(out, "%-3.3ls%10.10ls", lower_fence[fence], formatter(buf, bp->quantiles[1]));
    i = 14;
    ansi_colour_256(out, grid_colour);
    fputwc(L'│', out);
    i++;
    ansi_colour_256(out, bucket_colours[is_odd]);
    fputs(bp->label, out);
    i += label_len;
    ansi_colour_256(out, grid_colour);
    for (; i < 80 - (int) wcslen(min_label); i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fputws(min_label, out);
    ansi_colour_256(out, grid_colour);
    fputwc(L'│', out);
    ansi_reset(out);
    fputc('\n', out);

    // second line
    ansi_bgcolour_256(out, bucket_bgcolours[is_odd]);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fprintf(out, "%-3.3ls%10.10ls", L"25%", formatter(buf, bp->quantiles[2]));
    i = 14;
    ansi_colour_256(out, grid_colour);
    if (meta->pos[2] > 0) {
        fputwc(L'│', out);
        i++;
    }
    for (; i < 14 + meta->pos[2]; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    if (meta->show[2]) {
        ansi_colour_256(out, bucket_colours[is_odd]);
        fputwc(L'┌', out);
        i++;
        for (; i < 14 + meta->pos[3]; i++)
            fputwc(L'─', out);
        if (meta->show[3]) {
            fputwc(L'┬', out);
            i++;
        }
        for (; i < 14 + meta->pos[4]; i++)
            fputwc(L'─', out);
        fputwc(L'┐', out);
        i++;
    }
    ansi_colour_256(out, grid_colour);
    for (; i <= 80; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    ansi_reset(out);
    fputc('\n', out);

    // third line
    ansi_bgcolour_256(out, bucket_bgcolours[is_odd]);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fprintf(out, "%-3.3ls%10.10ls", L"50%", formatter(buf, bp->quantiles[3]));
    i = 14;
    ansi_colour_256(out, grid_colour);
    if (meta->pos[0] != 0 && meta->pos[1] > 0) {
        fputwc(L'│', out);
        i++;
    }
    for (; i < 14 + meta->pos[0]; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    if (meta->show[0]) {
        ansi_colour_256(out, bucket_colours[is_odd]);
        fputwc(meta->show[0], out);
        i++;
    }
    ansi_colour_256(out, grid_colour);
    for (; i < 14 + meta->pos[1]; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    if (meta->show[1]) {
        ansi_colour_256(out, bucket_colours[is_odd]);
        fputwc(meta->show[1], out);
        i++;
        for (; i < 14 + meta->pos[2]; i++)
            fputwc(L'─', out);
    }
    else {
        ansi_colour_256(out, grid_colour);
        for (; i < 14 + meta->pos[2]; i++)
            fputwc((i - 14) % 11 ? L' ' : L'│', out);
    }
    ansi_colour_256(out, bucket_colours[is_odd]);
    if (meta->show[2]) {
        fputwc(meta->show[2], out);
        i++;
    }
    for (; i < 14 + meta->pos[3]; i++)
        fputc(' ', out);
    if (meta->show[3]) {
        fputwc(meta->show[3], out);
        i++;
    }
    for (; i < 14 + meta->pos[4]; i++)
        fputc(' ', out);
    if (meta->show[4]) {
        fputwc(meta->show[4], out);
        i++;
    }
    if (meta->show[5]) {
        for (; i < 14 + meta->pos[5]; i++)
            fputwc(L'─', out);
        fputwc(meta->show[5], out);
        i++;
    }
    if (meta->show[6]) {
        ansi_colour_256(out, grid_colour);
        for (; i < 14 + meta->pos[6]; i++)
            fputwc((i - 14) % 11 ? L' ' : L'│', out);
        ansi_colour_256(out, bucket_colours[is_odd]);
        fputwc(meta->show[6], out);
        i++;
    }
    ansi_colour_256(out, grid_colour);
    for (; i <= 80; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    ansi_reset(out);
    fputc('\n', out);

    // fourth line
    ansi_bgcolour_256(out, bucket_bgcolours[is_odd]);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fprintf(out, "%-3.3ls%10.10ls", L"75%", formatter(buf, bp->quantiles[4]));
    i = 14;
    ansi_colour_256(out, grid_colour);
    if (meta->pos[2] > 0) {
        fputwc(L'│', out);
        i++;
    }
    for (; i < 14 + meta->pos[2]; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    if (meta->show[2]) {
        ansi_colour_256(out, bucket_colours[is_odd]);
        fputwc(L'└', out);
        i++;
        for (; i < 14 + meta->pos[3]; i++)
            fputwc(L'─', out);
        if (meta->show[3]) {
            fputwc(L'┴', out);
            i++;
        }
        for (; i < 14 + meta->pos[4]; i++)
            fputwc(L'─', out);
        fputwc(L'┘', out);
        i++;
    }
    ansi_colour_256(out, grid_colour);
    for (; i <= 80; i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    ansi_reset(out);
    fputc('\n', out);

    // fifth line
    ansi_bgcolour_256(out, bucket_bgcolours[is_odd]);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fprintf(out, "%-3.3ls%10.10ls", upper_fence[fence], formatter(buf, bp->quantiles[5]));
    i = 14;
    ansi_colour_256(out, grid_colour);
    fputwc(L'│', out);
    i++;
    r = swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"n=%" PRIu64,
                 (uint64_t) bp->n_samples);
    if (r > 0) {
        ansi_colour_256(out, bucket_colours[is_odd]);
        fputws(buf, out);
        i += r;
    }
    ansi_colour_256(out, grid_colour);
    for (; i < 80 - (int) wcslen(max_label); i++)
        fputwc((i - 14) % 11 ? L' ' : L'│', out);
    ansi_colour_256(out, bucket_colours[is_odd]);
    fputws(max_label, out);
    ansi_colour_256(out, grid_colour);
    fputwc(L'│', out);
    ansi_reset(out);
    fputc('\n', out);

    // ruler
//     fputws(L"             <123456789012345678901234567890"
//            L"12345678901234567890123456789012345>\n",
//            out);
//     fputws(L"             <         1         2         3"
//            L"         4         5         6     >\n",
//            out);
}

static void boxplot_print_footer(FILE *out)
{
    ansi_colour_256(out, grid_colour);
    fputws(L"─────────────┴──────────┴──────────┴────"
           L"──────┴──────────┴──────────┴──────────┘\n",
           out);
    ansi_reset(out);
}

void boxplot_print(const char *title,
                   const struct boxplot *boxplots,
                   size_t n_boxplots,
                   boxplot_format_sample_cb *formatter,
                   enum summary7_fence fence,
                   FILE *out)
{
    size_t i;
    double overall_min = INFINITY, overall_max = -INFINITY;
    double range, vpp, left_value, right_value;
    double grid_lines[7];
    struct boxplot_meta *bp_meta = NULL;
    int q;

    if (!formatter) formatter = &default_format_sample_cb;

// struct boxplot {
//     char *label;
//     double quantiles[5];
// };

    for (i = 0; i < n_boxplots; i++) {
        const struct boxplot *bp = &boxplots[i];

        for (q = 1; q < 7; q++) {
            if (isfinite(bp->quantiles[q])) {
                if (bp->quantiles[q] < overall_min)
                    overall_min = bp->quantiles[q];
                break;
            }
        }
        for (q = 5; q >= 0; q++) {
            if (isfinite(bp->quantiles[q])) {
                if (bp->quantiles[q] > overall_max)
                    overall_max = bp->quantiles[q];
                break;
            }
        }
    }

    assert(isfinite(overall_min));
    assert(isfinite(overall_max));
    assert(overall_max > overall_min);

    vpp = niceceil((overall_max - overall_min) / 66.0);
    range = 66.0 * vpp;
    left_value = overall_min - (range - (overall_max - overall_min)) / 2;
    if (left_value < 0 && overall_min >= 0)
        left_value = 0;
    right_value = left_value + range;

    fprintf(stderr, "overall_min=%g overall_max=%g overall_range=%g\n",
                    overall_min, overall_max, overall_max - overall_min);
    fprintf(stderr, "left_value=%g right_value=%g range=%g\n",
                    left_value, right_value, range);
    fprintf(stderr, "vpp=%g\n", vpp);

    for (i = 0; i < 7; i++) {
        grid_lines[i] = left_value + i * vpp * 11.0;
    }

    bp_meta = calloc(n_boxplots, sizeof(bp_meta[0]));

    for (i = 0; i < n_boxplots; i++) {
        const struct boxplot *bp = &boxplots[i];
        struct boxplot_meta *meta = &bp_meta[i];
        bool left_whisker, left_quartile, median, right_quartile, right_whisker;
        bool left_outlier, right_outlier;

        for (q = 0; q < 7; q++) {
            int pos = -1;

            if (isfinite(bp->quantiles[q]))
                pos = round((bp->quantiles[q] - left_value) / vpp);
            else if (bp->quantiles[q] == -INFINITY)
                pos = 0;
            else if (bp->quantiles[q] == INFINITY)
                pos = 66;

            meta->pos[q] = pos;
        }

        left_outlier = meta->pos[0] >= 0 && meta->pos[0] < meta->pos[1];
        left_whisker = meta->pos[1] >= 0 && meta->pos[1] < meta->pos[2];
        left_quartile = meta->pos[2] >= 0 && meta->pos[2] < meta->pos[4];
        median = meta->pos[3] >= 0 && ((meta->pos[3] > meta->pos[2] 
                                        && meta->pos[3] < meta->pos[4])
                                       || meta->pos[2] == meta->pos[4]);
        right_quartile = meta->pos[4] >= 0 && meta->pos[2] < meta->pos[4];
        right_whisker = meta->pos[5] >= 0 && ((right_quartile
                                               && meta->pos[5] > meta->pos[4])
                                              || meta->pos[5] > meta->pos[3]);
        right_outlier = meta->pos[6] >= 0 && meta->pos[6] > meta->pos[5];

        assert(left_quartile == right_quartile);

        if (left_outlier)
            meta->show[0] = meta->pos[0] >= 0 ? L'◊' : 0;

        if (left_whisker)
            meta->show[1] = isfinite(bp->quantiles[0]) ? L'├' : L'←';

        if (left_quartile)
            meta->show[2] = left_whisker ? L'┤' : L'│';

        if (median) {
            if (left_quartile)
                meta->show[3] = L'│';
            else if (left_whisker && !right_whisker)
                meta->show[3] = L'┤';
            else if (!left_whisker && right_whisker)
                meta->show[3] = L'├';
            else
                meta->show[3] = L'┼';
        }

        if (right_quartile)
            meta->show[4] = right_whisker ? L'├' : L'│';

        if (right_whisker)
            meta->show[5] = isfinite(bp->quantiles[6]) ? L'┤' : L'→';

        if (right_outlier)
            meta->show[6] = meta->pos[6] <= 66 ? L'◊' : 0;
    }

    boxplot_print_header(title, grid_lines, formatter, out);
    for (i = 0; i < n_boxplots; i++) {
        boxplot_print_one(&boxplots[i], &bp_meta[i],
                          formatter, fence,
                          i & 1, out);
    }
    boxplot_print_footer(out);
    // examine all boxplots to find non-infinite min and max
    // adjust for 7 grid labels like histogram
    // compute value-per-pip
    // compute pip position for quantiles

    // print header
    // print each boxplot
    // print footer

    free(bp_meta);
}
