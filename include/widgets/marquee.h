#ifndef GFX_WIDGETS_MARQUEE_H
#define GFX_WIDGETS_MARQUEE_H
#include "gfx.h"

/* -- Marquee ---- horizontally scrolling text. Tick advances the
 * scroll phase using fr_millis(); fits-text case is static. */
typedef struct {
    const GfxFont *font;
    GfxColor       color;
    GfxColor       bg_color;
    bool           skip_clear;
    const char    *text;
    int            speed_px_per_sec;
    int            gap_px;
    /* internal */
    int            phase;
    u32            last_tick_ms;
} GfxMarquee;

void GfxMarqueeDraw(GfxRenderingTile *tile, GfxMarquee *m);
int  GfxMarqueeTick(GfxMarquee *m);

#define NewGfxMarquee(...) \
    GfxNewWidget(sizeof(GfxMarquee), \
                 &(GfxMarquee){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxMarqueeDraw), \
                 GFX_TICK_FN(GfxMarqueeTick))

#endif
