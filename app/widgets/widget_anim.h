#ifndef WIDGET_ANIM_H
#define WIDGET_ANIM_H

#include "gfx.h"

#define ANIM_TRAIL 6

typedef struct {
    int          w, h;
    GfxColor     color;
    GfxColor     bg_color;
    /* Optional overlay background. If set, prev-frame trail erases
     * sample from bg->fb instead of writing bg_color, so the trail
     * appears to float over whatever the bg was painted with. The
     * library auto-ticks bg->source each frame; we re-blit the whole
     * bg whenever bg->version moves past what we last saw. Must
     * have the same w/h as the widget. */
    const GfxRenderTarget *bg;
    /* internal */
    u32 bg_seen_version;
    s16 pos_x, pos_y;
    s16 vel_x, vel_y;
    s16 trail_x[ANIM_TRAIL];
    s16 trail_y[ANIM_TRAIL];
    s16 prev_x[ANIM_TRAIL];
    s16 prev_y[ANIM_TRAIL];
    u8  trail_count;
    u8  prev_count;
    u32 last_tick_ms;
} AnimDemo;

void AnimDemoDraw(GfxRenderingTile *tile, AnimDemo *a);
int  AnimDemoTick(AnimDemo *a);

#define NewAnimDemo(...) \
    GfxNewWidget(sizeof(AnimDemo), &(AnimDemo){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(AnimDemoDraw), GFX_TICK_FN(AnimDemoTick))

#endif
