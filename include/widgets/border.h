#ifndef GFX_WIDGETS_BORDER_H
#define GFX_WIDGETS_BORDER_H
#include "gfx.h"

/* Border: rectangle outline as a widget. Wraps GfxDrawRect. */
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
