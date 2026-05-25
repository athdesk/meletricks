#ifndef WIDGET_BATTERY_H
#define WIDGET_BATTERY_H

#include "gfx.h"

typedef struct {
    const GfxFont *Font;
    GfxColor       Color;
    GfxColor       BgColor;
    bool           skip_clear;
    /* state — pulled from the SDK by BatteryBadgeBindCallback. */
    u8             battery_pct;
    bool           is_charging;
} BatteryBadge;

void BatteryBadgeDraw(GfxRenderingTile *tile, BatteryBadge *b);

/* Hook the SDK's battery push callback. Call once after construction;
 * only one BatteryBadge can use this. */
void BatteryBadgeBindCallback(GfxWidget *w);

#define NewBatteryBadge(...) \
    GfxNewWidget(sizeof(BatteryBadge), &(BatteryBadge){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(BatteryBadgeDraw), NULL)

GFX_DEFINE_APPLIER(BatteryBadge, Color)
GFX_DEFINE_APPLIER(BatteryBadge, BgColor)
GFX_DEFINE_APPLIER(BatteryBadge, Font)

#endif
