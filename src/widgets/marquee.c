#include "gfx.h"
#include "timer.h"

static int cycle_len_px(const GfxMarquee *m, int text_w, int box_w)
{
    int gap = (m->gap_px > 0) ? m->gap_px : (box_w / 2);
    return text_w + gap;
}

int GfxMarqueeTick(GfxMarquee *m)
{
    if (!m || !m->text || !m->font) return 0;
    int text_w = GfxTextWidth(m->font, m->text);

    u32 now = fr_millis();
    if (m->last_tick_ms == 0) { m->last_tick_ms = now; return 0; }

    int speed = (m->speed_px_per_sec > 0) ? m->speed_px_per_sec : 30;
    u32 elapsed = now - m->last_tick_ms;
    int adv = (int)((elapsed * (u32)speed) / 1000u);
    if (adv == 0) return 0;

    m->last_tick_ms += (u32)adv * 1000u / (u32)speed;
    int gap = (m->gap_px > 0) ? m->gap_px : 40;
    int cyc = text_w + gap;
    m->phase = (m->phase + adv) % cyc;
    return 1;
}

void GfxMarqueeDraw(GfxRenderingTile *tile, GfxMarquee *m)
{
    if (!m || !m->font || !m->text || tile->box.w <= 0) return;
    int h = (tile->box.h > 0) ? tile->box.h : m->font->line_height;

    if (!m->skip_clear) GfxFillRect(tile->fb, tile->box.x, tile->box.y, tile->box.w, h, m->bg_color);

    int text_w = GfxTextWidth(m->font, m->text);
    if (text_w <= tile->box.w) {
        int px = tile->box.x + (tile->box.w - text_w) / 2;
        GfxDrawTextBg(tile->fb, m->font, px, tile->box.y, m->text, m->color, m->bg_color);
        return;
    }

    int cyc = cycle_len_px(m, text_w, tile->box.w);
    int pen_x = tile->box.x + tile->box.w - m->phase;

    /* Two copies: the primary at pen_x (scrolling in from the right
     * during the early part of the cycle) and a repeat at pen_x - cyc
     * one full cycle to the left. The repeat fills the left half of
     * the screen while the primary scrolls off, and inherits the
     * primary's role at the wrap point — that gives unbroken motion
     * across `(phase + adv) % cyc`. */
    GfxClip saved = GfxFbPushClip(tile->fb, (GfxBoundingBox){tile->box.x, tile->box.y, tile->box.w, h});
    GfxDrawTextBg(tile->fb, m->font, pen_x,       tile->box.y, m->text, m->color, m->bg_color);
    GfxDrawTextBg(tile->fb, m->font, pen_x - cyc, tile->box.y, m->text, m->color, m->bg_color);
    GfxFbPopClip(tile->fb, saved);
}
