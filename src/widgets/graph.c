/* graph — line chart through `data[0..count-1]` with optional autoscale. */

#include "gfx.h"

static int fmt_int(char *buf, int v)
{
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    int neg = (v < 0);
    if (neg) v = -v;
    char tmp[12];
    int len = 0;
    while (v > 0) { tmp[len++] = (char)('0' + v % 10); v /= 10; }
    if (neg) tmp[len++] = '-';
    for (int i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = 0;
    return len;
}

void GfxGraphDraw(GfxRenderingTile *tile, GfxGraph *g)
{
    if (!g || !g->data || g->data_count <= 0 || tile->box.w <= 1 || tile->box.h <= 1) return;

    if (!g->skip_clear) GfxFillRect(tile->fb, tile->box.x, tile->box.y, tile->box.w, tile->box.h, g->bg_color);

    int dmin = g->data_min;
    int dmax = g->data_max;
    if (g->autoscale) {
        dmin = g->data[0];
        dmax = g->data[0];
        for (int i = 1; i < g->data_count; i++) {
            int v = g->data[i];
            if (v < dmin) dmin = v;
            if (v > dmax) dmax = v;
        }
    }
    if (dmax - dmin < 2) dmax = dmin + 2;

    int line_w    = (g->line_width > 0) ? g->line_width : 1;
    int line_half = line_w / 2;
    int plot_w    = tile->box.w - 1;
    int plot_h    = tile->box.h - 1;
    int span      = (g->data_count > 1) ? (g->data_count - 1) : 1;

    GfxClip saved = GfxFbPushClip(tile->fb, tile->box);


    if (g->show_legend) {
        int vals[3] = { dmax, dmin + (dmax - dmin) / 2, dmin };
        for (int li = 0; li < 3; li++) {
            int py = tile->box.y + plot_h
                   - (int)(((long)(vals[li] - dmin) * plot_h) / (dmax - dmin));
            int lx = tile->box.x, lw = tile->box.w;
            if (li == 1 && g->legend_font) {
                /* Leave a gap after the center label so the line doesn't cross it. */
                char buf[14];
                fmt_int(buf, vals[1]);
                int gap = GfxTextWidth(g->legend_font, buf) + 4;
                lx += gap;
                lw -= gap;
            }
            if (lw > 0) GfxHLine(tile->fb, lx, py, lw, g->legend_color);
        }
    }

    int prev_px = 0, prev_py = 0;
    for (int i = 0; i < g->data_count; i++) {
        int v = g->data[i];
        if (v < dmin) v = dmin;
        if (v > dmax) v = dmax;

        int px = tile->box.x + (i * plot_w) / span;
        int py = tile->box.y + plot_h
               - (int)(((long)(v - dmin) * plot_h) / (dmax - dmin));

        if (i > 0) {
            for (int dy = -line_half; dy < line_w - line_half; dy++) {
                GfxLine(tile->fb, prev_px, prev_py + dy, px, py + dy, g->color);
            }
        }
        prev_px = px;
        prev_py = py;
    }

    if (g->show_legend && g->legend_font) {
        int vals[3] = { dmax, dmin + (dmax - dmin) / 2, dmin };
        int lh = g->legend_font->line_height;
        char buf[14];
        for (int li = 0; li < 3; li++) {
            int py = tile->box.y + plot_h
                   - (int)(((long)(vals[li] - dmin) * plot_h) / (dmax - dmin));
            fmt_int(buf, vals[li]);
            /* Middle label: vertically centred on its line.
             * Top/bottom labels sit above their line. */
            int text_y = (li == 1) ? py - lh / 2 : py - lh;
            if (text_y < tile->box.y) text_y = tile->box.y;
            GfxDrawTextBg(tile->fb, g->legend_font, tile->box.x, text_y, buf,
                          g->legend_color, g->bg_color);
        }
    }

    GfxFbPopClip(tile->fb, saved);
}
