#ifndef GFX_WIDGETS_BORDER_H
#define GFX_WIDGETS_BORDER_H
#include "gfx_widgets.h"

/* Border: rectangle outline as a widget. Wraps GfxDrawRect. */
typedef struct {
    int      radius;
    int      thickness;
    GfxColor Color;
} GfxBorder;

void GfxBorderDraw(GfxRenderingTile *tile, GfxBorder *b);

#define NewGfxBorder(...) \
    GfxNewWidget(sizeof(GfxBorder), \
                 &(GfxBorder){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxBorderDraw), \
                 NULL)

#endif
