#ifndef GFX_WIDGETS_CLOCK_H
#define GFX_WIDGETS_CLOCK_H
#include "gfx.h"

#define GFX_CLOCK_MAX_SLOTS 8        /* HH:MM:SS */
typedef struct {
    const GfxFont *font;
    GfxColor       color;
    GfxColor       bg_color;
    bool           skip_clear;
    bool           show_seconds;
    GfxHalign      halign;
    GfxValign      valign;
    /* internal — partial-repaint cache */
    u32            last_unit;
    u8             slot_count;
    bool           cache_ready;                  /* positions + prev_text valid */
    char           prev_text[GFX_CLOCK_MAX_SLOTS + 1];
    s16            slot_x[GFX_CLOCK_MAX_SLOTS];  /* pen-x per slot       */
    s16            slot_w[GFX_CLOCK_MAX_SLOTS];  /* x_advance per slot   */
    s16            text_y;                       /* top of glyph row     */
} GfxClock;

void GfxClockDraw(GfxRenderingTile *tile, GfxClock *c);
int  GfxClockTick(GfxClock *c);

#define NewGfxClock(...) \
    GfxNewWidget(sizeof(GfxClock), \
                 &(GfxClock){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxClockDraw), \
                 GFX_TICK_FN(GfxClockTick))

#endif
