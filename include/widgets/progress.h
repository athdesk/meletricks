#ifndef GFX_WIDGETS_PROGRESS_H
#define GFX_WIDGETS_PROGRESS_H
#include "gfx_widgets.h"

typedef enum {
    GFX_PROGRESS_BAR     = 0,
    GFX_PROGRESS_BATTERY = 1,
    GFX_PROGRESS_GAUGE   = 2,
} GfxProgressStyle;

typedef struct {
    GfxColor         Color;
    GfxColor         BgColor;
    GfxColor         BorderColor;
    bool             show_border;
    int              value;
    int              max;
    GfxProgressStyle style;
    int              corner_radius;
    int              nub_w;
    int              start_deg;
    int              end_deg;
    int              thickness;
} GfxProgress;

void GfxProgressDraw(GfxRenderingTile *tile, GfxProgress *p);

#define NewGfxProgress(...) \
    GfxNewWidget(sizeof(GfxProgress), \
                 &(GfxProgress){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxProgressDraw), \
                 NULL)

GFX_DEFINE_APPLIER(GfxProgress, Color)
GFX_DEFINE_APPLIER(GfxProgress, BgColor)
GFX_DEFINE_APPLIER(GfxProgress, BorderColor)

#endif
