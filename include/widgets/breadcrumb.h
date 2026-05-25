#ifndef GFX_WIDGETS_BREADCRUMB_H
#define GFX_WIDGETS_BREADCRUMB_H
#include "gfx.h"

/* Breadcrumb: indicator which shows the current navigation stack */
typedef struct {
    int            fade_width;
    const GfxFont *font;
    GfxColor       color;
    GfxColor       bg_color;
    bool           skip_clear;
    const char    *separator;
} GfxBreadcrumb;

void GfxBreadcrumbDraw(GfxRenderingTile *tile, GfxBreadcrumb *bc);

#define NewGfxBreadcrumb(...) \
    GfxNewWidget(sizeof(GfxBreadcrumb), \
                 &(GfxBreadcrumb){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxBreadcrumbDraw), \
                 NULL)

#endif
