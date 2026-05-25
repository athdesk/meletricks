#include "gfx.h"

static void linear_fill(GfxFb *fb,
                        int inner_x, int inner_y, int inner_w, int inner_h,
                        int v, int max, GfxColor color, GfxColor bg)
{
    if (inner_w <= 0 || inner_h <= 0) return;
    int filled = (int)(((long)v * inner_w) / max);
    if (bg && filled < inner_w) {
        GfxFillRect(fb, inner_x + filled, inner_y,
                    inner_w - filled, inner_h, bg);
    }
    if (filled > 0) {
        GfxFillRect(fb, inner_x, inner_y, filled, inner_h, color);
    }
}

static void draw_bar(GfxFb *fb, const GfxProgress *p, GfxBoundingBox box, int v, int max)
{
    int inner_x = box.x, inner_y = box.y;
    int inner_w = box.w, inner_h = box.h;
    if (p->show_border) {
        GfxDrawRect(fb, box.x, box.y, &(GfxRect){
            .w = box.w, .h = box.h,
            .radius = p->corner_radius, .color = p->BorderColor,
        });
        inner_x++; inner_y++; inner_w -= 2; inner_h -= 2;
    }
    linear_fill(fb, inner_x, inner_y, inner_w, inner_h, v, max,
                p->Color, p->BgColor);
}

static void draw_battery(GfxFb *fb, const GfxProgress *p, GfxBoundingBox box, int v, int max)
{
    int nub_w  = (p->nub_w > 0) ? p->nub_w : 0;
    int body_w = box.w - nub_w;
    if (body_w <= 0) return;

    if (p->show_border) {
        GfxDrawRect(fb, box.x, box.y, &(GfxRect){
            .w = body_w, .h = box.h,
            .radius = p->corner_radius, .color = p->BorderColor,
        });
        if (nub_w > 0) {
            int nub_h = box.h / 2;
            if (nub_h < 4) nub_h = 4;
            int nub_y = box.y + (box.h - nub_h) / 2;
            GfxFillRect(fb, box.x + body_w, nub_y, nub_w, nub_h, p->BorderColor);
        }
    }

    int inset_min = p->show_border ? 2 : 0;
    int inset = (p->corner_radius > inset_min) ? p->corner_radius : inset_min;
    linear_fill(fb,
                box.x + inset, box.y + inset,
                body_w - 2 * inset, box.h - 2 * inset,
                v, max, p->Color, p->BgColor);
}

static void draw_gauge(GfxFb *fb, const GfxProgress *p, GfxBoundingBox box, int v, int max)
{
    int start = p->start_deg;
    int end   = p->end_deg;
    if (start == end) return;
    if (start > end) { int t = start; start = end; end = t; }

    int sweep = end - start;
    int thickness = (p->thickness > 0) ? p->thickness : (box.h / 5);
    if (thickness < 2) thickness = 2;

    int cx = box.x + box.w / 2;
    int cy = box.y + box.h / 2;
    int rmax = (box.w < box.h ? box.w : box.h) / 2 - 1;
    int r = rmax - thickness / 2;
    if (r <= 0) return;

    int filled_end = start + (int)(((long)v * sweep) / max);

    if (p->BgColor) {
        GfxArc(fb, cx, cy, r, start, end, thickness, p->BgColor);
    }
    if (filled_end > start) {
        GfxArc(fb, cx, cy, r, start, filled_end, thickness, p->Color);
    }
}

void GfxProgressDraw(GfxRenderingTile *tile, GfxProgress *p)
{
    if (!p || tile->box.w <= 0 || tile->box.h <= 0) return;

    int max = (p->max > 0) ? p->max : 1;
    int v   = p->value;
    if (v < 0)   v = 0;
    if (v > max) v = max;

    switch (p->style) {
    case GFX_PROGRESS_BATTERY: draw_battery(tile->fb, p, tile->box, v, max); break;
    case GFX_PROGRESS_GAUGE:   draw_gauge  (tile->fb, p, tile->box, v, max); break;
    case GFX_PROGRESS_BAR:
    default:                   draw_bar    (tile->fb, p, tile->box, v, max); break;
    }
}
