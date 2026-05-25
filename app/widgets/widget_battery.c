#include "widget_battery.h"
#include "kbd_event.h"

/* -- Local helpers (private to this TU). Mirror the pattern the old
 * StatusBar used for its right cluster; kept file-local because nothing
 * outside the widget should touch the icon/text formatter directly. */

/* Render a battery percentage into a 5-byte buffer as "NN%" / "100%".
 * Before the first 0x35 push from the SDK (pct == 0xFF), shows "--%"
 * so the widget reads as "no data yet" instead of "0%". */
static void format_pct(char *buf, u8 pct)
{
    if (pct == 0xFFu) {
        buf[0] = '-';
        buf[1] = '-';
        buf[2] = '%';
        buf[3] = 0;
        return;
    }
    if (pct > 100u) pct = 100u;
    char *p = buf;
    if (pct >= 100u) {
        *p++ = '1';
        *p++ = '0';
        *p++ = '0';
    } else if (pct >= 10u) {
        *p++ = (char)('0' + (pct / 10u));
        *p++ = (char)('0' + (pct % 10u));
    } else {
        *p++ = (char)('0' + pct);
    }
    *p++ = '%';
    *p   = 0;
}

/* 30 wide × 14 tall body + 3 wide × 6 tall terminal nub on the right.
 * (x, y) is the top-left of the body. Fill level is the inner area
 * scaled by `pct` (0..100). */
static void icon_battery(GfxFb *fb, int x, int y, u8 pct, u8 is_charging,
                         GfxColor c_fg, GfxColor c_bg)
{
    const int body_w = 30;
    const int body_h = 14;
    const int nub_w  = 3;
    const int nub_h  = 6;

    /* Clear the icon footprint to bg so leftover pixels from prior
     * frames don't leak through partial-fill gaps. */
    GfxFillRect(fb, x, y, body_w + nub_w, body_h, c_bg);

    /* Body outline (1 px). */
    GfxDrawRect(fb, x, y, &(GfxRect){ .w = body_w, .h = body_h, .color = c_fg });

    /* Terminal nub: small filled rect centred vertically on the body. */
    GfxFillRect(fb, x + body_w, y + (body_h - nub_h) / 2, nub_w, nub_h, c_fg);

    /* Fill: 1 px inset from the outline so border stays visible. */
    if (pct != 0xFFu) {
        u8 lvl = pct > 100u ? 100u : pct;
        int inner_w = body_w - 4;     /* 2 px inset on each side */
        int inner_h = body_h - 4;
        int fill_w  = (inner_w * (int)lvl) / 100;
        if (fill_w > 0) {
            GfxFillRect(fb, x + 2, y + 2, fill_w, inner_h, c_fg);
        }
    }

    if (is_charging == 1u) {
        int plus_cx = x + body_w / 2;
        int plus_cy = y + body_h / 2;
        GfxFillRect(fb, plus_cx - 4, plus_cy - 1, 8, 2, GFX_WHITE);
        GfxFillRect(fb, plus_cx - 1, plus_cy - 4, 2, 8, GFX_WHITE);
    }
}

/* -- SDK callback plumbing. Single supported instance; the callback
 * fires in IRQ context. Stores into u8 fields are atomic on Cortex-M;
 * GfxMarkDirty just sets a byte flag, also atomic. Worst case is a
 * lost-update race vs the main loop's draw-then-clear which the next
 * state change will correct — acceptable for this widget. */

static GfxWidget *s_self;

static void on_battery(const kbd_battery_t *bat)
{
    if (!s_self) return;
    BatteryBadge *b = s_self->data;
    int changed = 0;
    if (bat->battery_pct <= 100u && b->battery_pct != bat->battery_pct) {
        b->battery_pct = bat->battery_pct;
        changed = 1;
    }
    if (bat->is_charging != 0xFFu) {
        u8 ch = bat->is_charging ? 1 : 0;
        if (b->is_charging != ch) { b->is_charging = ch; changed = 1; }
    }
    if (changed) GfxMarkDirty(s_self);
}

void BatteryBadgeBindCallback(GfxWidget *w)
{
    s_self = w;
    if (!w) return;
    /* Sync the current SDK snapshot so we paint correct values from
     * the first frame instead of construction defaults. The callback
     * then handles subsequent diffs. */
    kbd_battery_t bat;
    kbd_battery_get(&bat);
    on_battery(&bat);
    GfxMarkDirty(w);
    kbd_battery_set_callback(on_battery);
}

void BatteryBadgeDraw(GfxRenderingTile *tile, BatteryBadge *b)
{
    if (!b || !b->font) return;
    if (!b->skip_clear) GfxFillTile(tile, b->bg_color);

    /* Battery icon sits on the right side of the rect; text fills the
     * space to its left, right-aligned against the icon. */
    int line_h = b->font->line_height;
    int cy     = tile->box.y + tile->box.h / 2;
    int text_y = cy - line_h / 2;

    int batt_w  = 30;
    int batt_y  = cy - 7;
    int right_x = tile->box.x + tile->box.w - 4;   /* small inset for terminal nub */
    int batt_x  = right_x - batt_w - 3;

    char pct_buf[5];
    format_pct(pct_buf, b->battery_pct);
    int pct_tw = GfxTextWidth(b->font, pct_buf);
    int text_x = batt_x - 4 - pct_tw;

    GfxDrawTextBg(tile->fb, b->font, text_x, text_y, pct_buf, b->color, b->bg_color);
    icon_battery(tile->fb, batt_x, batt_y, b->battery_pct, b->is_charging,
                 b->color, b->bg_color);
}
