/* clock — HH:MM[:SS] elapsed since boot via fr_millis(). Tick fires
 * on minute boundary (or second boundary when show_seconds is set).
 *
 * Repaint is incremental (differing glyph rects rather than a
 * full-area clear) */

#include "gfx.h"
#include "timer.h"
#include "firmware.h"

// #define TIME_GET() (fr_millis() % 86400000u)

static inline u32 time_get_fw() {
    lcd_rtc_live_t live;
    lcd_rtc_get_live(&live);
    u32 total_s = (live.hour * 3600u) + (live.minute * 60u) + live.second;
    return total_s * 1000u;
}

#define TIME_GET() time_get_fw()

static int format_clock(char *buf, u32 millis, int show_seconds)
{
    u32 total_s = millis / 1000u;
    u32 hours = total_s / 3600u;
    u32 mins  = (total_s / 60u) % 60u;
    u32 secs  = total_s % 60u;
    int n = 0;
    buf[n++] = (char)('0' + (hours / 10u) % 10u);
    buf[n++] = (char)('0' + (hours % 10u));
    buf[n++] = ':';
    buf[n++] = (char)('0' + (mins / 10u));
    buf[n++] = (char)('0' + (mins % 10u));
    if (show_seconds) {
        buf[n++] = ':';
        buf[n++] = (char)('0' + (secs / 10u));
        buf[n++] = (char)('0' + (secs % 10u));
    }
    buf[n] = 0;
    return n;
}

int GfxClockTick(GfxClock *c)
{
    if (!c) return 0;
    u32 div = c->show_seconds ? 1000u : 60000u;
    u32 unit = TIME_GET() / div;
    if (unit == c->last_unit) return 0;
    c->last_unit = unit;
    return 1;
}

/* Lay out slot positions once. Digit slots use the *max* advance
 * across '0'..'9' so the rendered text doesn't jitter as digits change
 * width using proportional fonts. Separator slots use the actual
 * separator glyph's advance. Glyphs are then centred within their
 * slot at draw time. */
static void clock_layout(GfxClock *c, GfxBoundingBox box, const char *text, int n)
{
    int digit_adv = 0;
    for (char d = '0'; d <= '9'; d++) {
        const GfxGlyph *g = GfxFontLookup(c->Font, (u8)d);
        int a = g ? g->x_advance : c->Font->default_advance;
        if (a > digit_adv) digit_adv = a;
    }

    int total_w = 0;
    s16 advs[GFX_CLOCK_MAX_SLOTS];
    for (int i = 0; i < n; i++) {
        char ch = text[i];
        int adv;
        if (ch >= '0' && ch <= '9') {
            adv = digit_adv;
        } else {
            const GfxGlyph *g = GfxFontLookup(c->Font, (u8)ch);
            adv = g ? g->x_advance : c->Font->default_advance;
        }
        advs[i] = (s16)adv;
        total_w += adv;
    }

    int line_h = c->Font->line_height;
    int x0;
    switch (c->halign) {
    case GFX_ALIGN_CENTER: x0 = box.x + (box.w - total_w) / 2; break;
    case GFX_ALIGN_RIGHT:  x0 = box.x + box.w - total_w;       break;
    case GFX_ALIGN_LEFT:
    default:               x0 = box.x;                         break;
    }
    int y0;
    switch (c->valign) {
    case GFX_VALIGN_MIDDLE: y0 = box.y + (box.h - line_h) / 2; break;
    case GFX_VALIGN_BOTTOM: y0 = box.y + box.h - line_h;       break;
    case GFX_VALIGN_TOP:
    default:                y0 = box.y;                        break;
    }

    int px = x0;
    for (int i = 0; i < n; i++) {
        c->slot_x[i] = (s16)px;
        c->slot_w[i] = advs[i];
        px += advs[i];
    }
    c->text_y     = (s16)y0;
    c->slot_count = (u8)n;
}

void GfxClockDraw(GfxRenderingTile *tile, GfxClock *c)
{
    if (!c || !c->Font) return;
    int line_h = c->Font->line_height;

    // We drop cache here since otherwise we'd only redraw the digit that changed
    if (GfxIsScreenChangeFrame()) c->cache_ready = 0;

    char buf[GFX_CLOCK_MAX_SLOTS + 1];
    int n = format_clock(buf, TIME_GET(), c->show_seconds);

    /* Centre a glyph's natural advance within its (potentially wider)
     * tabular slot — keeps narrow digits like '1' visually balanced in
     * a slot sized for the widest digit. */
    #define CLOCK_PEN_X(i, ch) ({                                               \
        const GfxGlyph *_g = GfxFontLookup(c->Font, (u8)(ch));                  \
        int _adv = _g ? _g->x_advance : c->Font->default_advance;               \
        c->slot_x[(i)] + (c->slot_w[(i)] - _adv) / 2;                           \
    })

    if (!c->cache_ready || c->slot_count != n) {
        if (!c->skip_clear) GfxFillTile(tile, c->BgColor);
        clock_layout(c, tile->box, buf, n);
        for (int i = 0; i < n; i++) {
            GfxDrawCharBg(tile->fb, c->Font, CLOCK_PEN_X(i, buf[i]), c->text_y,
                          (u8)buf[i], c->Color, c->BgColor);
            c->prev_text[i] = buf[i];
        }
        c->prev_text[n] = 0;
        c->cache_ready  = 1;
        return;
    }

    for (int i = 0; i < n; i++) {
        if (buf[i] == c->prev_text[i]) continue;
        GfxFillRect(tile->fb, c->slot_x[i], c->text_y, c->slot_w[i], line_h, c->BgColor);
        GfxDrawCharBg(tile->fb, c->Font, CLOCK_PEN_X(i, buf[i]), c->text_y,
                      (u8)buf[i], c->Color, c->BgColor);
        c->prev_text[i] = buf[i];
    }

    #undef CLOCK_PEN_X
}
