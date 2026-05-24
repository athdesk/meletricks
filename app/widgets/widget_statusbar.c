#include "widget_statusbar.h"
#include "gfx.h"
#include "kbd_event.h"
#include "wpm.h"
#include "timer.h"

#define WPM_ICON_W 72

static void icon_caps(GfxFb *fb, int x, int y, int caps_on,
                      const GfxFont *font, GfxColor c_active,
                      GfxColor c_dim, GfxColor c_bg)
{
    GfxColor c = caps_on ? c_active : c_dim;
    int icon_w = 24;
    int icon_h = 24;

    /* Two concentric non-rounded rects → 2-px outline. */
    GfxDrawRect(fb, x,     y,     &(GfxRect){ .w = icon_w,     .h = icon_h,     .radius = 0, .color = c });
    GfxDrawRect(fb, x + 1, y + 1, &(GfxRect){ .w = icon_w - 2, .h = icon_h - 2, .radius = 0, .color = c });

    if (!font) return;

    char buf[2];
    buf[0] = caps_on ? 'A' : 'a';
    buf[1] = 0;

    int text_x = x + (icon_w - GfxTextWidth(font, buf)) / 2;
    int text_y = y + (icon_h - font->line_height) / 2;

    GfxDrawCharBg(fb, font, text_x, text_y, (u8)buf[0], c, c_bg);
}

static void icon_transport(GfxFb *fb, int x, int y, u8 connection_type, const GfxFont *font, GfxColor c, GfxColor c_bg)
{
    int icon_w = 64;
    int icon_h = 24;

    /* Two concentric non-rounded rects → 2-px outline. */
    GfxDrawRect(fb, x,     y,     &(GfxRect){ .w = icon_w,     .h = icon_h,     .radius = 0, .color = c });
    GfxDrawRect(fb, x + 1, y + 1, &(GfxRect){ .w = icon_w - 2, .h = icon_h - 2, .radius = 0, .color = c });

    if (!font) return;

    char *buf;

    switch (connection_type) {
    case KBD_CONN_WIRED:   buf = "USB";   break;
    case KBD_CONN_2_4_GHZ: buf = "2.4G";  break;
    case KBD_CONN_BLE_1:   buf = "BLE1";  break;
    case KBD_CONN_BLE_2:   buf = "BLE2";  break;
    case KBD_CONN_BLE_3:   buf = "BLE3";  break;
    default:               buf = "N/A";   break;
    }

    int text_x = x + (icon_w - GfxTextWidth(font, buf)) / 2;
    int text_y = y + (icon_h - font->line_height) / 2;

    GfxDrawTextBg(fb, font, text_x, text_y, buf, c, c_bg);
}

static void icon_layer(GfxFb *fb, int x, int y, u8 layer, const GfxFont *font, GfxColor c, GfxColor c_bg)
{
    int icon_w = 32;
    int icon_h = 24;

    /* Two concentric non-rounded rects → 2-px outline. */
    GfxDrawRect(fb, x,     y,     &(GfxRect){ .w = icon_w,     .h = icon_h,     .radius = 0, .color = c });
    GfxDrawRect(fb, x + 1, y + 1, &(GfxRect){ .w = icon_w - 2, .h = icon_h - 2, .radius = 0, .color = c });

    if (!font) return;

    if (layer > 9) layer = 9;

    char buf[3];
    buf[0] = 'L';
    buf[1] = '0' + layer;
    buf[2] = 0;

    int text_x = x + (icon_w - GfxTextWidth(font, buf)) / 2;
    int text_y = y + (icon_h - font->line_height) / 2;

    GfxDrawTextBg(fb, font, text_x, text_y, buf, c, c_bg);
}

static void icon_wpm(GfxFb *fb, int x, int y, u32 v, const GfxFont *font,
                     GfxColor c, GfxColor c_bg)
{
    int icon_w = WPM_ICON_W;
    int icon_h = 24;

    GfxDrawRect(fb, x,     y,     &(GfxRect){ .w = icon_w,     .h = icon_h,     .radius = 0, .color = c });
    GfxDrawRect(fb, x + 1, y + 1, &(GfxRect){ .w = icon_w - 2, .h = icon_h - 2, .radius = 0, .color = c });

    if (!font) return;

    char buf[8];
    if (v > 999u) {
        buf[0]='F'; buf[1]='A'; buf[2]='S'; buf[3]='T';
        buf[4]=' '; buf[5]=':'; buf[6]=')'; buf[7]=0;
    } else {
        buf[0]='W'; buf[1]='P'; buf[2]='M'; buf[3]=' ';
        buf[4]='0'+(char)(v/100u);
        buf[5]='0'+(char)((v/10u)%10u);
        buf[6]='0'+(char)(v%10u);
        buf[7]=0;
    }

    int text_x = x + (icon_w - GfxTextWidth(font, buf)) / 2;
    int text_y = y + (icon_h - font->line_height) / 2;
    GfxDrawTextBg(fb, font, text_x, text_y, buf, c, c_bg);
}

static GfxWidget *s_self;       /* single supported instance */

static void on_kbd_status(const kbd_status_t *st)
{
    if (!s_self) return;
    StatusBar *s = s_self->data;
    int changed = 0;

    if (st->caps_lock_active != 0xFFu) {
        u8 caps = st->caps_lock_active ? 1 : 0;
        if (s->caps_on != caps) { s->caps_on = caps; changed = 1; }
    }
    if (st->connection_type != 0xFFu) {
        if (s->conn != st->connection_type) { s->conn = st->connection_type; changed = 1; }
    }
    if (st->kbd_layer != 0xFFu) {
        if (s->layer != st->kbd_layer) { s->layer = st->kbd_layer; changed = 1; }
    }

    if (changed) GfxMarkDirty(s_self);
}

void StatusBarBindCallbacks(GfxWidget *w)
{
    s_self = w;
    if (!w) return;

    /* Sync the current SDK snapshot so we paint correct values from
     * the first frame instead of the construction defaults. The
     * callback then handles subsequent diffs. */
    kbd_status_t st;
    kbd_status_get(&st);
    on_kbd_status(&st);
    /* on_kbd_status only marks dirty when fields *changed* vs the
     * struct's current values, so the initial sync via construction
     * defaults works fine. Forcing a dirty here doesn't hurt either:
     * the library will collapse it with the first real paint. */
    GfxMarkDirty(w);

    kbd_status_set_callback(on_kbd_status);
}

int StatusBarTick(StatusBar *s)
{
    u32 now = fr_millis();
    if (now - s->wpm_check_ms < 100u) return 0;
    s->wpm_check_ms = now;
    u32 v = wpm_current();
    if (v == s->wpm_value) return 0;
    s->wpm_value = v;
    return 1;
}

void StatusBarDraw(GfxRenderingTile *tile, StatusBar *s)
{
    if (!s || !s->font) return;

    int wpm_x = tile->box.x + tile->box.w - 8 - WPM_ICON_W;
    if (!s->skip_clear) {
        /* Left cluster — caps + transport + layer */
        GfxFillRect(tile->fb, tile->box.x, tile->box.y, 140, tile->box.h, s->bg_color);
        /* Right cluster — WPM icon (4 px left gap + icon + 8 px right margin) */
        GfxFillRect(tile->fb, wpm_x - 4, tile->box.y, WPM_ICON_W + 12, tile->box.h, s->bg_color);
    }

    int cy     = tile->box.y + tile->box.h / 2 + 1;  /* +1 keeps text below the divider */
    int icon_y = cy - 12;                              /* tallest icons are ~24-26 px */

    /* -- Left cluster ---------------------------------- */
    int x = tile->box.x + 10;
    icon_caps(tile->fb, x, icon_y, s->caps_on, s->font, s->color, s->color_dim, s->bg_color);
    x += 32;       /* icon_caps width 24 + 8 spacing */

    icon_transport(tile->fb, x, icon_y, s->conn, s->font, s->color_dim, s->bg_color);
    x += 72;       /* transport icon up to 64 wide + 8 spacing */

    icon_layer(tile->fb, x, icon_y, s->layer, s->font,
               (s->layer - 1) ? s->color : s->color_dim, s->bg_color);

    /* -- Right cluster --------------------------------- */
    GfxColor wpm_c = s->wpm_value ? s->color : s->color_dim;
    icon_wpm(tile->fb, wpm_x, icon_y, s->wpm_value, s->font, wpm_c, s->bg_color);
}
