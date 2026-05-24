/* BatteryBadge: percent text + battery icon. Sits in the top-right
 * corner of every demo screen and updates via the SDK's battery
 * push callback (no per-frame polling). */
#ifndef HELLO_WIDGET_BATTERY_H
#define HELLO_WIDGET_BATTERY_H

#include "gfx.h"

typedef struct {
    const GfxFont *font;
    GfxColor       color;
    GfxColor       bg_color;
    bool           skip_clear;
    /* state — pulled from the SDK by BatteryBadgeBindCallback. */
    u8             battery_pct;
    bool           is_charging;
} BatteryBadge;

void BatteryBadgeDraw(GfxRenderingTile *tile, BatteryBadge *b);

/* Hook the SDK's battery push callback. Call once after construction;
 * only one BatteryBadge instance is supported. */
void BatteryBadgeBindCallback(GfxWidget *w);

#define NewBatteryBadge(...) \
    GfxNewWidget(sizeof(BatteryBadge), &(BatteryBadge){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(BatteryBadgeDraw), NULL)

#endif
