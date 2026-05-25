/* NavBar: container widget that owns a small list of child GfxWidgets,
 * each paired with its own bounding box (GfxWidgetSlot). Its Draw
 * clears the bar, dispatches each child's draw via a correctly-scoped
 * GfxRenderingTile, and paints a 1-px separator hline along its bottom
 * edge. Its Tick forwards to children's ticks and aggregates the
 * dirty flag — any child that becomes dirty (either via its own tick
 * or via external code calling GfxMarkDirty on the child) propagates
 * to the NavBar so the library re-draws.
 *
 * Children are positioned by the GfxBoundingBox supplied to
 * NavBarAddChild; NavBar doesn't do layout, it just dispatches. */

#ifndef WIDGET_NAVBAR_H
#define WIDGET_NAVBAR_H

#include "gfx.h"

#define NAVBAR_MAX_CHILDREN 4

typedef struct {
    GfxColor      BgColor;
    GfxColor      SeparatorColor;
    bool          skip_clear;
    u8            child_count;
    GfxWidgetSlot slots[NAVBAR_MAX_CHILDREN];
} NavBar;

void NavBarDraw(GfxRenderingTile *tile, NavBar *nb);
int  NavBarTick(NavBar *nb);

/* Add a child after construction, paired with its screen-space box.
 * Silently drops if the inline cap is full. Children are drawn in
 * insertion order. */
void NavBarAddChild(GfxWidget *navbar_w, GfxWidget *child, GfxBoundingBox box);

#define NewNavBar(...) \
    GfxNewWidget(sizeof(NavBar), &(NavBar){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(NavBarDraw), GFX_TICK_FN(NavBarTick))

GFX_DEFINE_APPLIER(NavBar, BgColor)
GFX_DEFINE_APPLIER(NavBar, SeparatorColor)

#endif
