#include "gfx_shapes.h"
#include "mem.h"

void GfxFillRect(GfxFb *fb, int x, int y, int w, int h, GfxColor c)
{
    if (!GfxFbClipRect(fb, &x, &y, &w, &h)) return;
    u16 *row    = &fb->pixels[y * fb->width + x];
    int  stride = fb->width;
    while (h--) {
        GfxFillSpan(row, w, c);
        row += stride;
    }
}

/* sin(0°..90°) in Q12 (×4096). Reflected to cover 0..360°. */
static const u16 s_sin_q12[91] = {
       0,   71,  143,  214,  286,  357,  428,  499,  570,  641,
     711,  782,  852,  921,  991, 1060, 1129, 1198, 1266, 1334,
    1401, 1468, 1534, 1600, 1666, 1731, 1796, 1860, 1923, 1986,
    2048, 2110, 2171, 2231, 2290, 2349, 2408, 2465, 2522, 2578,
    2633, 2687, 2741, 2793, 2845, 2896, 2946, 2996, 3044, 3091,
    3138, 3183, 3228, 3271, 3314, 3355, 3396, 3435, 3474, 3511,
    3547, 3582, 3617, 3650, 3681, 3712, 3742, 3770, 3798, 3824,
    3849, 3873, 3896, 3917, 3937, 3956, 3974, 3991, 4006, 4021,
    4034, 4046, 4056, 4065, 4074, 4080, 4086, 4090, 4094, 4095,
    4096,
};

int GfxSinQ12(int deg)
{
    deg = ((deg % 360) + 360) % 360;
    if (deg <  90) return  s_sin_q12[deg];
    if (deg < 180) return  s_sin_q12[180 - deg];
    if (deg < 270) return -s_sin_q12[deg - 180];
    return                 -s_sin_q12[360 - deg];
}

int GfxCosQ12(int deg) { return GfxSinQ12(deg + 90); }

void GfxLine(GfxFb *fb, int x0, int y0, int x1, int y1, GfxColor c)
{
    int dx =  (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        GfxPixel(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void circle_outline(GfxFb *fb, int cx, int cy, int r, GfxColor c)
{
    int xi = 0, yi = r;
    int f = 1 - r;
    int ddf_x = 1, ddf_y = -2 * r;

    GfxPixel(fb, cx,     cy + r, c);
    GfxPixel(fb, cx,     cy - r, c);
    GfxPixel(fb, cx + r, cy,     c);
    GfxPixel(fb, cx - r, cy,     c);

    while (xi < yi) {
        if (f >= 0) { yi--; ddf_y += 2; f += ddf_y; }
        xi++; ddf_x += 2; f += ddf_x;

        GfxPixel(fb, cx + xi, cy + yi, c);
        GfxPixel(fb, cx - xi, cy + yi, c);
        GfxPixel(fb, cx + xi, cy - yi, c);
        GfxPixel(fb, cx - xi, cy - yi, c);
        GfxPixel(fb, cx + yi, cy + xi, c);
        GfxPixel(fb, cx - yi, cy + xi, c);
        GfxPixel(fb, cx + yi, cy - xi, c);
        GfxPixel(fb, cx - yi, cy - xi, c);
    }
}

static void circle_fill(GfxFb *fb, int cx, int cy, int r, GfxColor c)
{
    int xi = 0, yi = r;
    int f = 1 - r;
    int ddf_x = 1, ddf_y = -2 * r;

    GfxHLine(fb, cx - r, cy, 2 * r + 1, c);

    while (xi < yi) {
        if (f >= 0) { yi--; ddf_y += 2; f += ddf_y; }
        xi++; ddf_x += 2; f += ddf_x;

        GfxHLine(fb, cx - yi, cy - xi, 2 * yi + 1, c);
        GfxHLine(fb, cx - yi, cy + xi, 2 * yi + 1, c);
        if (xi != yi) {
            GfxHLine(fb, cx - xi, cy - yi, 2 * xi + 1, c);
            GfxHLine(fb, cx - xi, cy + yi, 2 * xi + 1, c);
        }
    }
}

void GfxDrawCircle(GfxFb *fb, int cx, int cy, const GfxCircle *c)
{
    if (!c || c->radius <= 0) return;
    if (c->fill) circle_fill   (fb, cx, cy, c->radius, c->color);
    else         circle_outline(fb, cx, cy, c->radius, c->color);
}

/* Per-pixel source sampler used by the GfxRestore* primitives. Skips
 * pixels outside src's bounds so a line/circle that overshoots the
 * source area leaves the destination untouched there. */
static inline void restore_pixel(GfxFb *fb, int px, int py,
                                 const GfxFb *src, int sox, int soy)
{
    int lx = px - sox;
    int ly = py - soy;
    if (lx < 0 || ly < 0 || lx >= src->width || ly >= src->height) return;
    GfxPixel(fb, px, py, src->pixels[ly * src->width + lx]);
}

static void restore_hline(GfxFb *fb, int x, int y, int w,
                          const GfxFb *src, int sox, int soy)
{
    if (w <= 0) return;
    if (y < fb->clip.y0 || y >= fb->clip.y1) return;
    if (x < fb->clip.x0) { w -= fb->clip.x0 - x; x = fb->clip.x0; }
    if (x + w > fb->clip.x1) w = fb->clip.x1 - x;
    if (w <= 0) return;
    int ly = y - soy;
    if (ly < 0 || ly >= src->height) return;
    int lx = x - sox;
    if (lx < 0) { w += lx; x -= lx; lx = 0; }
    if (lx + w > src->width) w = src->width - lx;
    if (w <= 0) return;
    u16       *dp = &fb->pixels [y * fb->width  + x];
    const u16 *sp = &src->pixels[ly * src->width + lx];
    memcpy(dp, sp, (u32)w * 2u);
}

void GfxRestoreLine(GfxFb *fb, int x0, int y0, int x1, int y1,
                    const GfxFb *src, int sox, int soy)
{
    if (!src || src == fb) return;
    int dx =  (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        restore_pixel(fb, x0, y0, src, sox, soy);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void GfxRestoreCircleFilled(GfxFb *fb, int cx, int cy, int r,
                            const GfxFb *src, int sox, int soy)
{
    if (!src || src == fb || r <= 0) return;
    int xi = 0, yi = r;
    int f = 1 - r;
    int ddf_x = 1, ddf_y = -2 * r;

    restore_hline(fb, cx - r, cy, 2 * r + 1, src, sox, soy);

    while (xi < yi) {
        if (f >= 0) { yi--; ddf_y += 2; f += ddf_y; }
        xi++; ddf_x += 2; f += ddf_x;
        restore_hline(fb, cx - yi, cy - xi, 2 * yi + 1, src, sox, soy);
        restore_hline(fb, cx - yi, cy + xi, 2 * yi + 1, src, sox, soy);
        if (xi != yi) {
            restore_hline(fb, cx - xi, cy - yi, 2 * xi + 1, src, sox, soy);
            restore_hline(fb, cx - xi, cy + yi, 2 * xi + 1, src, sox, soy);
        }
    }
}

void GfxArc(GfxFb *fb, int cx, int cy, int r,
             int start_deg, int end_deg, int thickness, GfxColor c)
{
    if (r <= 0 || thickness <= 0) return;
    if (start_deg > end_deg) {
        int t = start_deg; start_deg = end_deg; end_deg = t;
    }

    int t_half = thickness / 2;

    /* Step one degree at a time — sub-pixel arc length at our radii. */
    for (int deg = start_deg; deg <= end_deg; deg++) {
        int sx = GfxCosQ12(deg);
        int sy = GfxSinQ12(deg);
        for (int t = -t_half; t < thickness - t_half; t++) {
            int rr = r + t;
            int px = cx + ((sx * rr) >> 12);
            int py = cy - ((sy * rr) >> 12);    /* y axis flipped */
            GfxPixel(fb, px, py, c);
        }
    }
}

static void triangle_outline(GfxFb *fb,
                             int x0, int y0, int x1, int y1, int x2, int y2,
                             GfxColor c)
{
    GfxLine(fb, x0, y0, x1, y1, c);
    GfxLine(fb, x1, y1, x2, y2, c);
    GfxLine(fb, x2, y2, x0, y0, c);
}

static void triangle_fill(GfxFb *fb,
                          int x0, int y0, int x1, int y1, int x2, int y2,
                          GfxColor c)
{
    /* Sort vertices ascending in y. */
    int t;
    if (y1 < y0) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 < y0) { t = x0; x0 = x2; x2 = t; t = y0; y0 = y2; y2 = t; }
    if (y2 < y1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }

    if (y0 == y2) {                                 /* flat, degenerate */
        int xmin = x0, xmax = x0;
        if (x1 < xmin) xmin = x1;
        if (x1 > xmax) xmax = x1;
        if (x2 < xmin) xmin = x2;
        if (x2 > xmax) xmax = x2;
        GfxHLine(fb, xmin, y0, xmax - xmin + 1, c);
        return;
    }

    int dy02 = y2 - y0, dx02 = x2 - x0;

    if (y1 != y0) {
        int dy01 = y1 - y0, dx01 = x1 - x0;
        for (int y = y0; y <= y1; y++) {
            int xa = x0 + (dx01 * (y - y0)) / dy01;
            int xb = x0 + (dx02 * (y - y0)) / dy02;
            if (xa > xb) { int tt = xa; xa = xb; xb = tt; }
            GfxHLine(fb, xa, y, xb - xa + 1, c);
        }
    }
    if (y2 != y1) {
        int dy12 = y2 - y1, dx12 = x2 - x1;
        for (int y = y1 + 1; y <= y2; y++) {
            int xa = x1 + (dx12 * (y - y1)) / dy12;
            int xb = x0 + (dx02 * (y - y0)) / dy02;
            if (xa > xb) { int tt = xa; xa = xb; xb = tt; }
            GfxHLine(fb, xa, y, xb - xa + 1, c);
        }
    }
}

void GfxDrawTriangle(GfxFb *fb, const GfxTriangle *t)
{
    if (!t) return;
    if (t->fill) triangle_fill   (fb, t->x0, t->y0, t->x1, t->y1, t->x2, t->y2, t->color);
    else         triangle_outline(fb, t->x0, t->y0, t->x1, t->y1, t->x2, t->y2, t->color);
}

/* Rectangle outline. Sharp when radius == 0; otherwise corners are
 * traced as quarter arcs via GfxArc. Arc convention: 0° = +x,
 * 90° = top, growing CCW — so TL = 90..180, TR = 0..90, BL = 180..270,
 * BR = 270..360. */
void GfxDrawRect(GfxFb *fb, int x, int y, const GfxRect *r)
{
    if (!r || r->w <= 0 || r->h <= 0) return;

    int w = r->w;
    int h = r->h;
    GfxColor c = r->color;

    int rad = r->radius;
    if (rad < 0) rad = 0;
    int rmax = (w < h ? w : h) / 2;
    if (rad > rmax) rad = rmax;

    GfxHLine(fb, x + rad,     y,             w - 2 * rad, c);
    GfxHLine(fb, x + rad,     y + h - 1,     w - 2 * rad, c);
    GfxVLine(fb, x,           y + rad,       h - 2 * rad, c);
    GfxVLine(fb, x + w - 1,   y + rad,       h - 2 * rad, c);

    if (rad == 0) return;

    GfxArc(fb, x + rad,           y + rad,           rad,  90, 180, 1, c);
    GfxArc(fb, x + w - 1 - rad,   y + rad,           rad,   0,  90, 1, c);
    GfxArc(fb, x + rad,           y + h - 1 - rad,   rad, 180, 270, 1, c);
    GfxArc(fb, x + w - 1 - rad,   y + h - 1 - rad,   rad, 270, 360, 1, c);
}
