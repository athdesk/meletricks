/* menu_list — scrolling vertical menu with optional indicator column. */

#include "gfx.h"

/* Hidden-item bookkeeping. `hidden_mask` covers items[0..31]; indices
 * past 32 are always visible. is_hidden is the single point that reads
 * the mask so the cap is encoded in one place. */
static inline int is_hidden(const GfxMenuList *m, int idx)
{
    if (!m || idx < 0 || idx >= m->item_count) return 0;
    if (idx >= 32) return 0;
    return (m->hidden_mask >> idx) & 1u;
}

int GfxMenuListIsHidden(const GfxMenuList *m, int idx)
{
    return is_hidden(m, idx);
}

/* Walk forward from `from` to the next visible item, wrapping once.
 * Returns -1 if every item is hidden (caller keeps current selected). */
static int next_visible(const GfxMenuList *m, int from)
{
    if (!m || m->item_count <= 0) return -1;
    for (int step = 1; step <= m->item_count; step++) {
        int idx = (from + step) % m->item_count;
        if (!is_hidden(m, idx)) return idx;
    }
    return -1;
}

static int prev_visible(const GfxMenuList *m, int from)
{
    if (!m || m->item_count <= 0) return -1;
    for (int step = 1; step <= m->item_count; step++) {
        int idx = (from - step + m->item_count) % m->item_count;
        if (!is_hidden(m, idx)) return idx;
    }
    return -1;
}

static void select_prev(GfxMenuList *m)
{
    if (!m || m->item_count <= 0) return;
    int n = prev_visible(m, m->selected);
    if (n >= 0) m->selected = n;
}

static void select_next(GfxMenuList *m)
{
    if (!m || m->item_count <= 0) return;
    int n = next_visible(m, m->selected);
    if (n >= 0) m->selected = n;
}

void GfxMenuListSetHidden(GfxWidget *w, int idx, int hidden)
{
    if (!w || !w->data) return;
    GfxMenuList *m = (GfxMenuList *)w->data;
    if (idx < 0 || idx >= 32) return;       /* outside mask range: no-op */
    u32 bit = 1u << idx;
    u32 prev = m->hidden_mask;
    m->hidden_mask = hidden ? (prev | bit) : (prev & ~bit);
    if (m->hidden_mask == prev) return;     /* no state change */

    /* If we just hid the selected row, jump to the nearest visible one
     * — prefer forward (matches DOWN-arrow expectations), then back. */
    if (hidden && m->selected == idx) {
        int n = next_visible(m, idx);
        if (n < 0) n = prev_visible(m, idx);
        if (n >= 0) m->selected = n;
    }
    GfxMarkDirty(w);
}

static void adjust(GfxMenuList *m, int dir)
{
    if (!m || !m->editing || m->item_count <= 0) return;
    if (m->selected < 0 || m->selected >= m->item_count) return;
    if (is_hidden(m, m->selected)) return;
    const GfxMenuItem *it = &m->items[m->selected];
    if (it->type == GFX_MENU_SLIDER) {
        if (!it->slider.get || !it->slider.set) return;
        int step = (it->slider.step > 0) ? it->slider.step : 1;
        int v = it->slider.get() + (dir >= 0 ? step : -step);
        if (v < it->slider.min) v = it->slider.min;
        if (v > it->slider.max) v = it->slider.max;
        it->slider.set(v);
    } else if (it->type == GFX_MENU_CHOICE) {
        if (!it->choice.get || !it->choice.set) return;
        int n = it->choice.option_count;
        if (n <= 0) return;
        int idx = it->choice.get() + (dir >= 0 ? 1 : -1);
        if (idx < 0)  idx = n - 1;     /* wrap both ways */
        if (idx >= n) idx = 0;
        it->choice.set(idx);
    }
}

/* -- Command handlers -------------------------------------------- */

void GfxMenuListUp(GfxMenuList *m)
{
    if (!m) return;
    if (m->editing) adjust(m, -1);
    else            select_prev(m);
}

void GfxMenuListDown(GfxMenuList *m)
{
    if (!m) return;
    if (m->editing) adjust(m, +1);
    else            select_next(m);
}

void GfxMenuListEnter(GfxWidget *w)
{
    if (!w) return;
    GfxMenuList *m = w->data;
    if (!m || m->item_count <= 0) return;
    if (m->selected < 0 || m->selected >= m->item_count) return;
    const GfxMenuItem *it = &m->items[m->selected];

    if (m->editing) {
        m->editing = 0;
        GfxMarkDirty(w);
        return;
    }
    switch (it->type) {
    case GFX_MENU_SLIDER:
    case GFX_MENU_CHOICE:
        m->editing = 1;
        GfxMarkDirty(w);
        break;
    case GFX_MENU_TOGGLE:
        if (it->toggle.get && it->toggle.set) {
            it->toggle.set(!it->toggle.get());
            GfxMarkDirty(w);
        }
        break;
    case GFX_MENU_LINK:
        if (it->link.target) GfxNavTo((GfxScreen *)it->link.target);
        break;
    case GFX_MENU_ACTION:
        if (it->action.activate) it->action.activate();
        break;
    }
}

/* Returns 1 if it consumed the event (was in edit mode, now exited),
 * 0 if the caller should handle back-navigation (e.g. GfxNavBack). */
int GfxMenuListBack(GfxWidget *w)
{
    if (!w) return 0;
    GfxMenuList *m = w->data;
    if (!m || !m->editing) return 0;
    m->editing = 0;
    GfxMarkDirty(w);
    return 1;
}

static int format_int(char *buf, int v)
{
    int n = 0;
    if (v < 0) { buf[n++] = '-'; v = -v; }
    char digits[10];
    int dn = 0;
    do { digits[dn++] = (char)('0' + v % 10); v /= 10; } while (v && dn < 10);
    while (dn > 0) buf[n++] = digits[--dn];
    buf[n] = 0;
    return n;
}

/* Count of non-hidden items. */
static int visible_count(const GfxMenuList *m)
{
    int n = 0;
    for (int i = 0; i < m->item_count; i++) if (!is_hidden(m, i)) n++;
    return n;
}

/* Map an items[] index -> its position among visible items, or -1 if
 * the item itself is hidden. Indicator dots use this so the selected
 * dot tracks the visible row, not the raw items[] slot. */
static int visible_pos_of(const GfxMenuList *m, int idx)
{
    if (idx < 0 || idx >= m->item_count || is_hidden(m, idx)) return -1;
    int pos = 0;
    for (int i = 0; i < idx; i++) if (!is_hidden(m, i)) pos++;
    return pos;
}

static void draw_indicators(GfxFb *fb, const GfxMenuList *m,
                            GfxBoundingBox box,
                            int vis_total, int vis_scroll, int rows_drawn,
                            int pad)
{
    /* Anchor the indicator column to the visible text region, not the
     * widget box — `box.h` is usually slightly larger than `rows_drawn *
     * per_item` (rounding slack) and the user expects arrows flush
     * with the first / last text row, not floating off in the slack. */
    int per_item = m->font->line_height + m->item_spacing;
    int text_top = box.y;
    int text_bot = box.y + (rows_drawn - 1) * per_item + m->font->line_height;

    int tri_half = 6;
    int tri_h    = 6;
    int tri_cx   = (m->indicator_align == GFX_MENU_INDICATOR_RIGHT)
                 ? (box.x + box.w - pad / 2)
                 : (box.x + pad / 2);

    GfxColor up_c = (vis_scroll > 0)
                  ? m->color_indicator
                  : m->color_indicator_inactive;
    GfxDrawTriangle(fb, &(GfxTriangle){
        .x0 = tri_cx - tri_half, .y0 = text_top + tri_h,
        .x1 = tri_cx + tri_half, .y1 = text_top + tri_h,
        .x2 = tri_cx,            .y2 = text_top,
        .fill = 1, .color = up_c,
    });

    GfxColor dn_c = (vis_scroll + rows_drawn < vis_total)
                  ? m->color_indicator
                  : m->color_indicator_inactive;
    GfxDrawTriangle(fb, &(GfxTriangle){
        .x0 = tri_cx - tri_half, .y0 = text_bot - tri_h,
        .x1 = tri_cx + tri_half, .y1 = text_bot - tri_h,
        .x2 = tri_cx,            .y2 = text_bot,
        .fill = 1, .color = dn_c,
    });

    if (vis_total > 1) {
        int top_y = text_top + tri_h + 8;
        int bot_y = text_bot - tri_h - 8;
        int span  = bot_y - top_y;
        if (span < 0) span = 0;
        int sel_pos = visible_pos_of(m, m->selected);
        for (int i = 0; i < vis_total; i++) {
            int dy = top_y + (i * span) / (vis_total - 1);
            int is_sel = (i == sel_pos);
            int sz = is_sel ? 5 : 2;
            GfxColor dc = is_sel ? m->color_indicator
                                 : m->color_indicator_inactive;
            GfxFillRect(fb, tri_cx - sz / 2, dy - sz / 2, sz, sz, dc);
        }
    }
}

void GfxMenuListDraw(GfxRenderingTile *tile, GfxMenuList *m)
{
    if (!m || !m->font || m->item_count <= 0) return;

    if (!m->skip_clear) GfxFillTile(tile, m->bg_color);

    int line_h    = m->font->line_height;
    int per_item  = line_h + m->item_spacing;
    int max_fit   = tile->box.h / per_item;
    if (max_fit < 1) max_fit = 1;
    /* All scroll/overflow accounting is in terms of *visible* items —
     * hidden rows neither claim a row on screen nor count toward the
     * "needs scrolling" decision. */
    int vis_total    = visible_count(m);
    if (vis_total <= 0) {
        /* Nothing to draw, but bg fill already happened. */
        return;
    }
    int has_overflow = (vis_total > max_fit);
    int visible   = has_overflow ? max_fit : vis_total;

    if (m->selected < 0)              m->selected = 0;
    if (m->selected >= m->item_count) m->selected = m->item_count - 1;
    /* Selected could land on a hidden item if mask was changed without
     * going through SetHidden — snap to the next visible. */
    if (is_hidden(m, m->selected)) {
        int n = next_visible(m, m->selected);
        if (n < 0) n = prev_visible(m, m->selected);
        if (n >= 0) m->selected = n;
    }

    /* `m->scroll` is the visible-row offset of the first row on screen,
     * not an items[] index. Translating it once here keeps the per-row
     * loop simple. */
    int sel_pos = visible_pos_of(m, m->selected);
    if (sel_pos < 0) sel_pos = 0;
    if (sel_pos < m->scroll) {
        m->scroll = sel_pos;
    } else if (sel_pos >= m->scroll + visible) {
        m->scroll = sel_pos - visible + 1;
    }
    if (m->scroll < 0) m->scroll = 0;
    if (m->scroll > vis_total - visible) m->scroll = vis_total - visible;
    if (m->scroll < 0) m->scroll = 0;

    int has_indicator = (m->indicator_align != GFX_MENU_INDICATOR_NONE);
    int pad           = has_indicator ? m->indicator_pad : 0;
    int text_x        = (m->indicator_align == GFX_MENU_INDICATOR_LEFT)
                      ? (tile->box.x + pad)
                      : tile->box.x;
    int text_w        = tile->box.w - pad;
    if (text_w < 0) text_w = 0;

    GfxClip saved = GfxFbPushClip(tile->fb, (GfxBoundingBox){text_x, tile->box.y, text_w, tile->box.h});

    int row_y = tile->box.y;
    int rows_drawn = 0;
    /* Walk items[] skipping hidden ones, drop the first `m->scroll`
     * visible entries, then paint up to `visible` rows. */
    int vis_seen = 0;
    for (int idx = 0; idx < m->item_count && rows_drawn < visible; idx++) {
        if (is_hidden(m, idx)) continue;
        if (vis_seen++ < m->scroll) continue;
        const GfxMenuItem *item = &m->items[idx];

        int is_sel       = (idx == m->selected);
        int is_slider    = (item->type == GFX_MENU_SLIDER);
        int is_choice    = (item->type == GFX_MENU_CHOICE);
        int is_editable  = is_slider || is_choice;
        GfxColor c       = is_sel ? m->color_selected : m->color_normal;

        /* Cursor: '*' while editing an editable row, '>' otherwise. */
        char buf[64];
        int n = 0;
        buf[n++] = is_sel ? ((m->editing && is_editable) ? '*' : '>') : ' ';
        buf[n++] = ' ';
        for (const char *p = item->label; *p && n < 60; p++) buf[n++] = *p;

        if (is_slider) {
            buf[n] = 0;
            GfxDrawTextBg(tile->fb, m->font, text_x, row_y, buf, c, m->bg_color);

            /* Fixed-width slider: always exactly half the row width,
             * right-aligned beside a fixed-width value column. Bars
             * stay column-aligned across rows even when label widths
             * differ. */
            int val_w  = 36;
            int bar_w  = text_w / 2;
            int bar_h  = 6;
            int val_x  = text_x + text_w - val_w;
            int bar_x  = val_x - 6 - bar_w;
            int bar_y  = row_y + (m->font->line_height - bar_h) / 2;
            int avail  = bar_w;

            /* Slider stays in the inactive colour until ENTER puts
             * the row into edit mode — selection alone shouldn't
             * suggest the value will move on UP/DOWN. */
            GfxColor bar_col = (is_sel && m->editing)
                             ? m->color_indicator
                             : m->color_indicator_inactive;
            GfxDrawRect(tile->fb, bar_x, bar_y, &(GfxRect){
                .w = avail, .h = bar_h, .color = bar_col,
            });
            int vmin = item->slider.min;
            int vmax = item->slider.max;
            if (vmax <= vmin) vmax = vmin + 1;
            int v = item->slider.get ? item->slider.get() : vmin;
            if (v < vmin) v = vmin;
            if (v > vmax) v = vmax;
            int inner_w = avail - 2;
            int filled  = ((v - vmin) * inner_w) / (vmax - vmin);
            if (filled > 0) {
                GfxFillRect(tile->fb, bar_x + 1, bar_y + 1, filled, bar_h - 2, bar_col);
            }

            char vbuf[12];
            format_int(vbuf, v);
            int vtw = GfxTextWidth(m->font, vbuf);
            GfxDrawTextBg(tile->fb, m->font, val_x + (val_w - vtw), row_y, vbuf, c, m->bg_color);
        } else if (item->type == GFX_MENU_TOGGLE && item->toggle.get) {
            buf[n++] = ':';
            buf[n++] = ' ';
            const char *s = item->toggle.get() ? "on" : "off";
            while (*s && n < 60) buf[n++] = *s++;
            buf[n] = 0;
            GfxDrawTextBg(tile->fb, m->font, text_x, row_y, buf, c, m->bg_color);
        } else if (is_choice && item->choice.get && item->choice.options) {
            buf[n] = 0;
            GfxDrawTextBg(tile->fb, m->font, text_x, row_y, buf, c, m->bg_color);

            int opt_n = item->choice.option_count;
            int v = item->choice.get();
            if (v < 0) v = 0;
            if (v >= opt_n) v = (opt_n > 0) ? opt_n - 1 : 0;
            const char *opt = (opt_n > 0) ? item->choice.options[v] : "?";

            char rbuf[40];
            int rn = 0;
            int show_arrows = is_sel && m->editing;
            if (show_arrows) { rbuf[rn++] = '<'; rbuf[rn++] = ' '; }
            for (const char *p = opt; *p && rn < 36; p++) rbuf[rn++] = *p;
            if (show_arrows) { rbuf[rn++] = ' '; rbuf[rn++] = '>'; }
            rbuf[rn] = 0;

            /* Value column reads as "data not a label" — dim it to
             * the inactive accent unless we're actively editing
             * (where bright accent + < > markers signal "you're
             * driving this"). */
            GfxColor rc = show_arrows ? m->color_indicator
                                      : m->color_indicator_inactive;
            int rw = GfxTextWidth(m->font, rbuf);
            GfxDrawTextBg(tile->fb, m->font, text_x + text_w - rw, row_y, rbuf, rc, m->bg_color);
        } else {
            buf[n] = 0;
            GfxDrawTextBg(tile->fb, m->font, text_x, row_y, buf, c, m->bg_color);
        }

        row_y += per_item;
        rows_drawn++;
    }

    GfxFbPopClip(tile->fb, saved);

    /* Indicators only show when overflow is actually possible — for
     * a short menu that fits in `h`, arrows + dots would just be
     * misleading chrome. */
    if (has_indicator && has_overflow)
        draw_indicators(tile->fb, m, tile->box, vis_total, m->scroll, rows_drawn, pad);
}
