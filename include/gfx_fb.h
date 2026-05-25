#ifndef GFX_FB_H
#define GFX_FB_H

#include "fr_types.h"
#include "gfx_color.h"

// Don't build this type directly, treat as opaque.
typedef struct { s16 x0, y0, x1, y1; } GfxClip;

typedef struct GfxFb {
    u16    *pixels;
    s16     width;
    s16     height;
    /* Internal. Use GfxFbPushClip/GfxFbPopClip to scope drawing to a sub-region. */
    GfxClip clip;
} GfxFb;

/* Different type for basically the same information as GfxClip, to avoid mixing them up.
 * Also users are more likely to need w/h info instead of corner info. */
typedef struct { int x, y, w, h; } GfxBoundingBox;

/* -- Lifecycle -------------------------------------------------------- */

void GfxFbInit (GfxFb *fb, u16 *pixels, int w, int h);
void GfxFbClear(GfxFb *fb, GfxColor c);

GfxClip GfxFbPushClip(GfxFb *fb, GfxBoundingBox box);
void    GfxFbPopClip (GfxFb *fb, GfxClip saved);

/* -- Clip helpers ----------------------------------------------------- *
 * All three mutate their arguments in place and return 1 if any area
 * survives clipping, 0 if the region is fully outside the clip rect. */

/* Clip a destination rectangle. */
static inline int GfxFbClipRect(const GfxFb *fb,
                                 int *x, int *y, int *w, int *h)
{
    if (*x < fb->clip.x0) { *w -= fb->clip.x0 - *x; *x = fb->clip.x0; }
    if (*y < fb->clip.y0) { *h -= fb->clip.y0 - *y; *y = fb->clip.y0; }
    if (*x + *w > fb->clip.x1) *w = fb->clip.x1 - *x;
    if (*y + *h > fb->clip.y1) *h = fb->clip.y1 - *y;
    return (*w > 0 && *h > 0);
}

/* Clip a destination rectangle and advance source offsets to match. */
static inline int GfxFbClipRectSrc(const GfxFb *fb,
                                    int *x, int *y, int *w, int *h,
                                    int *sx, int *sy)
{
    if (*x < fb->clip.x0) { int d = fb->clip.x0 - *x; *sx += d; *w -= d; *x = fb->clip.x0; }
    if (*y < fb->clip.y0) { int d = fb->clip.y0 - *y; *sy += d; *h -= d; *y = fb->clip.y0; }
    if (*x + *w > fb->clip.x1) *w = fb->clip.x1 - *x;
    if (*y + *h > fb->clip.y1) *h = fb->clip.y1 - *y;
    return (*w > 0 && *h > 0);
}

/* Clip an x0/y0/x1/y1 box. */
static inline int GfxFbClipBox(const GfxFb *fb,
                                int *x0, int *y0, int *x1, int *y1)
{
    if (*x0 < fb->clip.x0) *x0 = fb->clip.x0;
    if (*y0 < fb->clip.y0) *y0 = fb->clip.y0;
    if (*x1 > fb->clip.x1) *x1 = fb->clip.x1;
    if (*y1 > fb->clip.y1) *y1 = fb->clip.y1;
    return (*x1 > *x0 && *y1 > *y0);
}

/* Fill `w` contiguous pixels at `dst` with colour `c`.
 * Uses the STMIA burst-store path; faster than a scalar loop for any width >= 4. */
void GfxFillSpan(u16 *dst, int w, GfxColor c);

static inline void GfxPixel(GfxFb *fb, int x, int y, GfxColor c)
{
    if (x < fb->clip.x0 || x >= fb->clip.x1) return;
    if (y < fb->clip.y0 || y >= fb->clip.y1) return;
    fb->pixels[y * fb->width + x] = c;
}

static inline void GfxHLine(GfxFb *fb, int x, int y, int w, GfxColor c)
{
    if (w <= 0) return;
    if (y < fb->clip.y0 || y >= fb->clip.y1) return;
    if (x < fb->clip.x0) { w -= fb->clip.x0 - x; x = fb->clip.x0; }
    if (x + w > fb->clip.x1) w = fb->clip.x1 - x;
    if (w <= 0) return;
    GfxFillSpan(&fb->pixels[y * fb->width + x], w, c);
}

static inline void GfxVLine(GfxFb *fb, int x, int y, int h, GfxColor c)
{
    if (h <= 0) return;
    if (x < fb->clip.x0 || x >= fb->clip.x1) return;
    if (y < fb->clip.y0) { h -= fb->clip.y0 - y; y = fb->clip.y0; }
    if (y + h > fb->clip.y1) h = fb->clip.y1 - y;
    if (h <= 0) return;
    u16 *p = &fb->pixels[y * fb->width + x];
    int stride = fb->width;
    while (h--) { *p = c; p += stride; }
}

/* -- Blit ------------------------------------------------------------- */

void GfxBlitFb(GfxFb *fb, int x, int y, const GfxFb *src);

void GfxBlit1Bpp(GfxFb *fb, int x, int y, int w, int h,
                 const u8 *src, int src_stride_bytes, GfxColor color);

void GfxBlit2Bpp(GfxFb *fb, int x, int y, int w, int h,
                 const u8 *src, int src_stride_bytes,
                 GfxColor fg, GfxColor bg);

typedef struct { struct GfxFb *fb; GfxBoundingBox box; } GfxRenderingTile;

static inline void GfxFillTile(GfxRenderingTile *tile, GfxColor c)
{
    GfxFb *fb = tile->fb;
    int x = tile->box.x, y = tile->box.y, w = tile->box.w, h = tile->box.h;
    if (!GfxFbClipRect(fb, &x, &y, &w, &h)) return;
    u16 *row = &fb->pixels[y * fb->width + x];
    while (h--) { GfxFillSpan(row, w, c); row += fb->width; }
}

#endif /* GFX_FB_H */
