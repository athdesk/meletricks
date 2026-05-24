#include "widget_anim.h"
#include "wpm.h"
#include "timer.h"

/* Partial redraw: erase last frame's trail dots, then paint new ones.
 * Skipping the full-box clear drops per-frame PSRAM writes by ~20x;
 * the library's screen-change wipe keeps first-paint clean. On screen
 * change, if a bg is set, blit it over the widget area so the trail
 * draws over the user-supplied image instead of cleared pixels. */
void AnimDemoDraw(GfxRenderingTile *tile, AnimDemo *a)
{
    if (a->bg
        && (GfxIsScreenChangeFrame() || a->bg_seen_version != a->bg->version))
    {
        GfxBlitFb(tile->fb, tile->box.x, tile->box.y, a->bg->fb);
        /* bg covered the old trail too, so there's nothing to erase. */
        a->prev_count       = 0;
        a->bg_seen_version  = a->bg->version;
    }

    for (u8 i = 0; i < a->prev_count; i++) {
        int radius = 2 + i;
        if (a->bg) {
            GfxRestoreCircleFilled(tile->fb,
                                   tile->box.x + a->prev_x[i],
                                   tile->box.y + a->prev_y[i],
                                   radius,
                                   a->bg->fb, tile->box.x, tile->box.y);
        } else {
            GfxDrawCircle(tile->fb,
                          tile->box.x + a->prev_x[i],
                          tile->box.y + a->prev_y[i],
                          &(GfxCircle){
                              .radius = radius, .fill = 1, .color = a->bg_color,
                          });
        }
    }
    for (u8 i = 0; i < a->trail_count; i++) {
        GfxDrawCircle(tile->fb,
                      tile->box.x + a->trail_x[i],
                      tile->box.y + a->trail_y[i],
                      &(GfxCircle){
                          .radius = 2 + i, .fill = 1, .color = a->color,
                      });
        a->prev_x[i] = a->trail_x[i];
        a->prev_y[i] = a->trail_y[i];
    }
    a->prev_count = a->trail_count;
}

int AnimDemoTick(AnimDemo *a)
{
    u32 now = fr_millis();
    if (a->last_tick_ms == 0) {
        a->pos_x = (s16)(a->w / 2);
        a->pos_y = (s16)(a->h / 2);
        a->vel_x = 3;
        a->vel_y = 2;
        a->last_tick_ms = now;
        return 1;
    }
    if (now - a->last_tick_ms < 50) return 0;
    a->last_tick_ms = now;

    /* Idle drift at (1,1); typing kicks to (3,2) then scales with WPM.
     * Y stays ~2/3 of X so the bounce reads as a clear diagonal. */
    u32 wpm = wpm_current();
    int vx, vy;
    if (wpm == 0u) { vx = 1; vy = 1; }
    else {
        vx = 3 + (int)(wpm / 15u);
        vy = 2 + (int)(wpm / 22u);
    }
    /* Cap so a fast burst can't tunnel through a wall in one step. */
    if (vx > 14) vx = 14;
    if (vy > 10) vy = 10;
    a->vel_x = (s16)((a->vel_x < 0) ? -vx : vx);
    a->vel_y = (s16)((a->vel_y < 0) ? -vy : vy);

    if (a->trail_count < ANIM_TRAIL) {
        a->trail_x[a->trail_count] = a->pos_x;
        a->trail_y[a->trail_count] = a->pos_y;
        a->trail_count++;
    } else {
        for (u8 i = 0; i < ANIM_TRAIL - 1; i++) {
            a->trail_x[i] = a->trail_x[i + 1];
            a->trail_y[i] = a->trail_y[i + 1];
        }
        a->trail_x[ANIM_TRAIL - 1] = a->pos_x;
        a->trail_y[ANIM_TRAIL - 1] = a->pos_y;
    }

    a->pos_x += a->vel_x;
    a->pos_y += a->vel_y;

    int m = 8;
    if (a->pos_x < m)        { a->pos_x = (s16)(m);        a->vel_x = -a->vel_x; }
    if (a->pos_x > a->w - m) { a->pos_x = (s16)(a->w - m); a->vel_x = -a->vel_x; }
    if (a->pos_y < m)        { a->pos_y = (s16)(m);        a->vel_y = -a->vel_y; }
    if (a->pos_y > a->h - m) { a->pos_y = (s16)(a->h - m); a->vel_y = -a->vel_y; }
    return 1;
}
