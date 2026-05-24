#ifndef GFX_WIDGETS_BORDER_H
#define GFX_WIDGETS_BORDER_H
#include "gfx.h"

/* -- Border ---- rectangle outline as a widget. Wraps GfxDrawRect.
 * Doesn't clear its area (it's a thin outline); paint over whatever's
 * already there. `thickness` defaults to 1 — values > 1 stack
 * concentric outlines stepped inward by 1 px each, with the radius
 * decreasing in lock-step. */
typedef struct {
    int      radius;
    int      thickness;
    GfxColor color;
} GfxBorder;

void GfxBorderDraw(GfxRenderingTile *tile, GfxBorder *b);

#define NewGfxBorder(...) \
    GfxNewWidget(sizeof(GfxBorder), \
                 &(GfxBorder){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxBorderDraw), \
                 NULL)

#endif
