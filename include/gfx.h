#ifndef GFX_H
#define GFX_H

#include "fr_types.h"
#include "gfx_color.h"
#include "gfx_font.h"

#define GFX_CORNER_RADIUS 22

typedef struct GfxFb {
    u16 *pixels;
    s16  width;
    s16  height;
    /* Active clip rectangle, half-open: [clip_x0, clip_x1) × [clip_y0,
     * clip_y1). GfxFbInit() resets this to the full panel; all
     * primitives intersect their writes against it. Use
     * GfxFbPushClip/GfxFbPopClip to scope drawing to a sub-region.
     * TODO: figure out if this is causing perf issues with rastering
     *       operations (like fonts) */
    s16  clip_x0, clip_y0, clip_x1, clip_y1;
} GfxFb;

/* Saved clip from GfxFbPushClip. Treat it as an opaque value, restore via pop. */
typedef struct {
    s16 x0, y0, x1, y1;
} GfxClip;

#include "gfx_screen.h"

void GfxFbInit (GfxFb *fb, u16 *pixels, int w, int h);
void GfxFbClear(GfxFb *fb, GfxColor c);

GfxClip GfxFbPushClip(GfxFb *fb, GfxBoundingBox box);
void    GfxFbPopClip (GfxFb *fb, GfxClip saved);

// -- Drawing primitives -----------------------------------------------

static inline void GfxPixel(GfxFb *fb, int x, int y, GfxColor c)
{
    if (x < fb->clip_x0 || x >= fb->clip_x1) return;
    if (y < fb->clip_y0 || y >= fb->clip_y1) return;
    fb->pixels[y * fb->width + x] = c;
}

static inline void GfxHLine(GfxFb *fb, int x, int y, int w, GfxColor c)
{
    if (w <= 0) return;
    if (y < fb->clip_y0 || y >= fb->clip_y1) return;
    if (x < fb->clip_x0) { w -= fb->clip_x0 - x; x = fb->clip_x0; }
    if (x + w > fb->clip_x1) w = fb->clip_x1 - x;
    if (w <= 0) return;
    u16 *p = &fb->pixels[y * fb->width + x];
    while (w--) *p++ = c;
}

static inline void GfxVLine(GfxFb *fb, int x, int y, int h, GfxColor c)
{
    if (h <= 0) return;
    if (x < fb->clip_x0 || x >= fb->clip_x1) return;
    if (y < fb->clip_y0) { h -= fb->clip_y0 - y; y = fb->clip_y0; }
    if (y + h > fb->clip_y1) h = fb->clip_y1 - y;
    if (h <= 0) return;
    u16 *p = &fb->pixels[y * fb->width + x];
    int stride = fb->width;
    while (h--) { *p = c; p += stride; }
}

void GfxFillRect(GfxFb *fb, int x, int y, int w, int h, GfxColor c);

void GfxBlitFb(GfxFb *fb, int x, int y, const GfxFb *src);

void GfxBlit1Bpp(GfxFb *fb, int x, int y, int w, int h,
                 const u8 *src, int src_stride_bytes, GfxColor color);

void GfxBlit2Bpp(GfxFb *fb, int x, int y, int w, int h,
                 const u8 *src, int src_stride_bytes,
                 GfxColor fg, GfxColor bg);

int  GfxDrawChar (GfxFb *fb, const GfxFont *f, int x, int y, u8 cp, GfxColor color);
int  GfxDrawText (GfxFb *fb, const GfxFont *f, int x, int y, const char *s, GfxColor color);
int  GfxTextWidth(const GfxFont *f, const char *s);
const GfxGlyph *GfxFontLookup(const GfxFont *f, u8 cp);

/* Bg-aware variants: Needed fpr 2bpp (alpha).
 * Caller is responsible for having filled the destination with `bg`
 * the 2-bpp path skips transparent source pixels rather than
 * touching the destination. */
int GfxDrawCharBg(GfxFb *fb, const GfxFont *f, int x, int y, u8 cp,
                  GfxColor fg, GfxColor bg);
int GfxDrawTextBg(GfxFb *fb, const GfxFont *f, int x, int y, const char *s,
                  GfxColor fg, GfxColor bg);

// TODO: Pixel background aware Text rendering functions

typedef enum {
    GFX_ALIGN_LEFT   = 0,
    GFX_ALIGN_CENTER = 1,
    GFX_ALIGN_RIGHT  = 2,
} GfxHalign;

typedef enum {
    GFX_VALIGN_TOP    = 0,
    GFX_VALIGN_MIDDLE = 1,
    GFX_VALIGN_BOTTOM = 2,
} GfxValign;

void GfxDrawImage(GfxFb *fb, int x, int y, int w, int h,
                  const u16 *pixels, GfxColor transparent_key);
void GfxDrawAnim (GfxFb *fb, int x, int y, int w, int h,
                  const u16 *frames_pixels, int frame_count,
                  int frame_index, GfxColor transparent_key);
void GfxLine(GfxFb *fb, int x0, int y0, int x1, int y1, GfxColor c);
void GfxArc (GfxFb *fb, int cx, int cy, int r,
             int start_deg, int end_deg, int thickness, GfxColor c);

/* Restore-by-shape: walk the same rasterization path as GfxLine /
 * filled GfxDrawCircle but copy pixels from `src` instead of writing a
 * constant colour. (src_origin_x, src_origin_y) is the fb coordinate
 * where src's (0, 0) pixel lives. Pixels outside src are left alone.
 * Used for performance reasons (partial restore of a background under
 * a changing overlay). */
void GfxRestoreLine        (GfxFb *fb, int x0, int y0, int x1, int y1,
                            const GfxFb *src, int src_origin_x, int src_origin_y);
void GfxRestoreCircleFilled(GfxFb *fb, int cx, int cy, int radius,
                            const GfxFb *src, int src_origin_x, int src_origin_y);

// Trigonometry
int GfxSinQ12(int deg);
int GfxCosQ12(int deg);

typedef struct { int w, h, radius; GfxColor color; } GfxRect;
void GfxDrawRect(GfxFb *fb, int x, int y, const GfxRect *r);

typedef struct { int radius, fill; GfxColor color; } GfxCircle;
void GfxDrawCircle(GfxFb *fb, int cx, int cy, const GfxCircle *c);

typedef struct {
    int x0, y0, x1, y1, x2, y2;
    int fill;
    GfxColor color;
} GfxTriangle;
void GfxDrawTriangle(GfxFb *fb, const GfxTriangle *t);

/* -- Widget headers ----------------------------------------------- */
#include "widgets/border.h"
#include "widgets/textbox.h"
#include "widgets/menu_list.h"
#include "widgets/clock.h"
#include "widgets/graph.h"
#include "widgets/progress.h"
#include "widgets/marquee.h"
#include "widgets/breadcrumb.h"
#include "widgets/icon.h"
#include "widgets/carousel.h"

#endif
