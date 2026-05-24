/* border — rectangle outline widget. Stacks `thickness` concentric
 * rounded outlines, each stepped one pixel inward with the radius
 * tracking the step so the curves stay parallel. */

#include "gfx.h"

void GfxBorderDraw(GfxRenderingTile *tile, GfxBorder *b)
{
    if (!b) return;
    int t = (b->thickness > 0) ? b->thickness : 1;
    for (int i = 0; i < t; i++) {
        int r = b->radius - i;
        if (r < 0) r = 0;
        GfxDrawRect(tile->fb, tile->box.x + i, tile->box.y + i, &(GfxRect){
            .w = tile->box.w - 2 * i, .h = tile->box.h - 2 * i,
            .radius = r, .color = b->color,
        });
    }
}
