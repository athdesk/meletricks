#include "widget_navbar.h"

void NavBarAddChild(GfxWidget *navbar_w, GfxWidget *child, GfxBoundingBox box)
{
    if (!navbar_w || !child) return;
    NavBar *nb = navbar_w->data;
    if (nb->child_count < NAVBAR_MAX_CHILDREN) {
        nb->slots[nb->child_count].widget = child;
        nb->slots[nb->child_count].box    = box;
        nb->child_count++;
    }
}

int NavBarTick(NavBar *nb)
{
    if (!nb) return 0;
    int dirty = 0;
    for (int i = 0; i < nb->child_count; i++) {
        GfxWidget *c = nb->slots[i].widget;
        if (!c) continue;
        if (c->tick && c->tick(c->data)) c->dirty = true;
        if (c->dirty) dirty = true;
    }
    return dirty;
}

void NavBarDraw(GfxRenderingTile *tile, NavBar *nb)
{
    if (!nb) return;
    if (!nb->skip_clear) GfxFillTile(tile, nb->bg_color);

    /* Dispatch children. Always paints each child on a NavBar redraw,
     * since the library can only see NavBar's dirty flag and a single
     * dirty child has dirtied the whole container. Tighter per-child
     * dirty tracking isn't worth the complexity for a 2-3 child top bar. */
    for (int i = 0; i < nb->child_count; i++) {
        GfxWidgetSlot *slot = &nb->slots[i];
        GfxWidget     *c    = slot->widget;
        if (!c || !c->draw) continue;
        GfxRenderingTile child_tile = { tile->fb, slot->box };
        c->draw(&child_tile, c->data);
        c->dirty = 0;
    }

    /* Hairline separator along the bottom edge of the bar. Stays
     * inside the NavBar's bounding rect; insetted from the corners to
     * match the existing top-separator placement. */
    GfxHLine(tile->fb,
             tile->box.x + 6, tile->box.y + tile->box.h - 1,
             tile->box.w - 12, nb->separator_color);
}
