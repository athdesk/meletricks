#include "gfx.h"

static int avail_h(const GfxCarousel *c)
{
    int label_h = c->font ? (int)c->font->line_height : 0;
    int h = c->h - label_h - 6;   /* 2px bottom pad + 4px icon-label gap */
    return h > 0 ? h : 0;
}

/* Slide distance that guarantees both icons clear the viewport at the
 * start/end of the animation, regardless of each icon's individual width. */
static int slide(const GfxCarousel *c, int in_w, int out_w)
{
    int max_w = in_w > out_w ? in_w : out_w;
    return (c->w + max_w) / 2;
}

/* Shared setup for Next/Prev: computes and caches icon widths and slide
 * distance so GfxCarouselDraw does not repeat this work every frame. */
static void start_anim(GfxCarousel *c, int old_idx, int dir)
{
    int ah    = avail_h(c);
    int in_w  = GfxIconWidth(&c->items[c->selected].icon, ah);
    int out_w = GfxIconWidth(&c->items[old_idx].icon,     ah);
    int sl    = slide(c, in_w, out_w);
    c->_cached_in_w  = in_w;
    c->_cached_out_w = out_w;
    c->_cached_slide = sl;
    c->_anim_offset  = dir * sl;
}

void GfxCarouselNext(GfxCarousel *c)
{
    if (!c || c->item_count <= 0) return;
    int old = c->selected;
    c->selected = (c->selected + 1) % c->item_count;
    start_anim(c, old, +1);
}

void GfxCarouselPrev(GfxCarousel *c)
{
    if (!c || c->item_count <= 0) return;
    int old = c->selected;
    c->selected = (c->selected - 1 + c->item_count) % c->item_count;
    start_anim(c, old, -1);
}

void GfxCarouselSelect(GfxCarousel *c)
{
    if (!c || !c->on_select || c->item_count <= 0) return;
    c->on_select((GfxCarouselItem *)&c->items[c->selected]);
}

int GfxCarouselTick(void *data)
{
    GfxCarousel *c = (GfxCarousel *)data;
    if (!c) return 0;

    if (c->anim_speed <= 0) {
        c->_anim_offset = 0;
        return 0;
    }

    int was_animating = (c->_anim_offset != 0);

    if (c->_anim_offset > 0) {
        int step = c->_anim_offset / 3;
        if (step < c->anim_speed) step = c->anim_speed;
        c->_anim_offset -= step;
        if (c->_anim_offset < 0) c->_anim_offset = 0;
    } else if (c->_anim_offset < 0) {
        int step = (-c->_anim_offset) / 3;
        if (step < c->anim_speed) step = c->anim_speed;
        c->_anim_offset += step;
        if (c->_anim_offset > 0) c->_anim_offset = 0;
    }

    return was_animating ? 1 : 0;
}

void GfxCarouselDraw(GfxRenderingTile *tile, GfxCarousel *c)
{
    if (!c || c->item_count <= 0) return;

    GfxClip saved = GfxFbPushClip(tile->fb, tile->box);

    if (!c->skip_clear)
        GfxFillRect(tile->fb, tile->box.x, tile->box.y, c->w, c->h, c->bg_color);

    int label_h  = c->font ? (int)c->font->line_height : 0;
    int ly       = tile->box.y + c->h - label_h - 2;
    int ah       = avail_h(c);
    int icon_y   = tile->box.y;
    int animating = (c->_anim_offset != 0);

    /* Incoming (selected) icon — use cached width during animation to
     * avoid recomputing GfxIconWidth (and get_text) every frame. */
    int in_w = animating ? c->_cached_in_w
                         : GfxIconWidth(&c->items[c->selected].icon, ah);
    int in_x = tile->box.x + (c->w - in_w) / 2 + c->_anim_offset;
    GfxIconDraw(tile->fb, &c->items[c->selected].icon,
                in_x, icon_y, ah, c->color, c->bg_color);

    /* Outgoing icon — only drawn while animating; widths already cached. */
    if (animating) {
        int out_idx = (c->_anim_offset > 0)
            ? (c->selected - 1 + c->item_count) % c->item_count
            : (c->selected + 1) % c->item_count;
        int dir   = (c->_anim_offset > 0) ? 1 : -1;
        int out_x = tile->box.x + (c->w - c->_cached_out_w) / 2
                    + c->_anim_offset - dir * c->_cached_slide;
        GfxIconDraw(tile->fb, &c->items[out_idx].icon,
                    out_x, icon_y, ah, c->color, c->bg_color);
    }

    /* Label centred below the icon area. */
    if (c->font && c->items[c->selected].label) {
        const char *label = c->items[c->selected].label;
        int lw = GfxTextWidth(c->font, label);
        int lx = tile->box.x + (c->w - lw) / 2;
        GfxDrawTextBg(tile->fb, c->font, lx, ly, label, c->color, c->bg_color);
    }

    GfxFbPopClip(tile->fb, saved);
}
