#ifndef GFX_SHAPES_H
#define GFX_SHAPES_H

#include "gfx_fb.h"

/* Default corner radius for rounded rectangles. */
#define GFX_CORNER_RADIUS 22

/* -- Filled primitives ------------------------------------------------ */

void GfxFillRect(GfxFb *fb, int x, int y, int w, int h, GfxColor c);

/* -- Line and arc ----------------------------------------------------- */

void GfxLine(GfxFb *fb, int x0, int y0, int x1, int y1, GfxColor c);
void GfxArc (GfxFb *fb, int cx, int cy, int r,
             int start_deg, int end_deg, int thickness, GfxColor c);

/* -- Restore-by-shape ------------------------------------------------- *
 * Walk the same rasterisation path as GfxLine / filled GfxDrawCircle
 * but copy pixels from `src` instead of writing a constant colour.
 * (src_origin_x, src_origin_y) is the fb coordinate where src's (0,0)
 * pixel lives.  Pixels outside src are left untouched. */
void GfxRestoreLine        (GfxFb *fb, int x0, int y0, int x1, int y1,
                            const GfxFb *src, int src_origin_x, int src_origin_y);
void GfxRestoreCircleFilled(GfxFb *fb, int cx, int cy, int radius,
                            const GfxFb *src, int src_origin_x, int src_origin_y);

/* -- Trigonometry ----------------------------------------------------- *
 * Integer sin/cos in Q12 fixed-point (result range -4096..+4096).     */
int GfxSinQ12(int deg);
int GfxCosQ12(int deg);

/* -- Shape structs and draw calls ------------------------------------- */

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

#endif /* GFX_SHAPES_H */
