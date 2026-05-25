#include "gfx.h"

const GfxGlyph *GfxFontLookup(const GfxFont *f, u8 cp)
{
    if (cp < f->first_cp || cp > f->last_cp) return NULL;
    u8 slot = f->index[cp - f->first_cp];
    if (slot == GFX_GLYPH_ABSENT) return NULL;
    return &f->glyphs[slot];
}

int GfxDrawChar(GfxFb *fb, const GfxFont *f,
                  int x, int y, u8 cp, GfxColor color)
{
    const GfxGlyph *g = GfxFontLookup(f, cp);
    if (!g) return x + f->default_advance;

    if (g->width && g->height) {
        if (f->flags & GFX_FONT_FLAG_2BPP) {
            int stride = (g->width + 3) >> 2;
            GfxBlit2Bpp(fb,
                        x + g->x_off,
                        y + f->baseline + g->y_off,
                        g->width, g->height,
                        &f->bitmap[g->bitmap_off],
                        stride, color, 0);
        } else {
            int stride = (g->width + 7) >> 3;
            GfxBlit1Bpp(fb,
                        x + g->x_off,
                        y + f->baseline + g->y_off,
                        g->width, g->height,
                        &f->bitmap[g->bitmap_off],
                        stride, color);
        }
    }
    return x + g->x_advance;
}

int GfxDrawText(GfxFb *fb, const GfxFont *f,
                  int x, int y, const char *s, GfxColor color)
{
    while (*s) {
        x = GfxDrawChar(fb, f, x, y, (u8)*s++, color);
    }
    return x;
}

int GfxDrawCharBg(GfxFb *fb, const GfxFont *f,
                  int x, int y, u8 cp, GfxColor fg, GfxColor bg)
{
    const GfxGlyph *g = GfxFontLookup(f, cp);
    if (!g) return x + f->default_advance;

    if (g->width && g->height) {
        if (f->flags & GFX_FONT_FLAG_2BPP) {
            int stride = (g->width + 3) >> 2;   /* 4 px / byte */
            GfxBlit2Bpp(fb,
                        x + g->x_off,
                        y + f->baseline + g->y_off,
                        g->width, g->height,
                        &f->bitmap[g->bitmap_off],
                        stride, fg, bg);
        } else {
            int stride = (g->width + 7) >> 3;
            GfxBlit1Bpp(fb,
                        x + g->x_off,
                        y + f->baseline + g->y_off,
                        g->width, g->height,
                        &f->bitmap[g->bitmap_off],
                        stride, fg);
            (void)bg;
        }
    }
    return x + g->x_advance;
}

int GfxDrawTextBg(GfxFb *fb, const GfxFont *f,
                  int x, int y, const char *s, GfxColor fg, GfxColor bg)
{
    while (*s) {
        x = GfxDrawCharBg(fb, f, x, y, (u8)*s++, fg, bg);
    }
    return x;
}

int GfxTextWidth(const GfxFont *f, const char *s)
{
    int w = 0;
    while (*s) {
        const GfxGlyph *g = GfxFontLookup(f, (u8)*s++);
        w += g ? g->x_advance : f->default_advance;
    }
    return w;
}

typedef struct {
    const char *end;    /* one past last char to render on this line */
    const char *next;   /* start of next line (skips break whitespace) */
    int         width;  /* pixel width of the chars in [start, end) */
} line_info_t;

/* Find the end of the next line of text starting at s, given a max line
 * width of box_w. Word-wraps at spaces; hard-breaks long words that don't
 * fit at all. Respects '\n' as a forced break. */
static line_info_t find_next_line(const GfxFont *f, const char *s, int box_w)
{
    line_info_t info = { s, s, 0 };
    const char *p = s;
    int last_space_pos    = -1;
    int last_space_width  = 0;
    int width             = 0;

    while (*p) {
        if (*p == '\n') {
            info.end   = p;
            info.next  = p + 1;
            info.width = width;
            return info;
        }
        const GfxGlyph *g = GfxFontLookup(f, (u8)*p);
        int adv = g ? g->x_advance : f->default_advance;

        if (width + adv > box_w) {
            if (last_space_pos >= 0) {
                info.end   = s + last_space_pos;
                info.next  = s + last_space_pos + 1;
                info.width = last_space_width;
                while (*info.next == ' ') info.next++;
                return info;
            }
            if (p == s) {
                /* Single char wider than the box — force one char per line
                 * so we don't loop forever. */
                info.end   = p + 1;
                info.next  = p + 1;
                info.width = adv;
            } else {
                info.end   = p;
                info.next  = p;
                info.width = width;
            }
            return info;
        }

        if (*p == ' ') {
            last_space_pos   = (int)(p - s);
            last_space_width = width;
        }
        width += adv;
        p++;
    }
    info.end   = p;
    info.next  = p;
    info.width = width;
    return info;
}

/* Render a textbox into the tile's bounding box — position comes from
 * the tile. Word-wraps at spaces; hard-breaks words wider than the box;
 * respects '\n' as a forced line break. Fills with bg_color first when set. */
void GfxTextboxDraw(GfxRenderingTile *tile, GfxTextbox *tb)
{
    if (!tb || !tb->text || !tb->Font || tile->box.w <= 0) return;

    int line_h = tb->Font->line_height;
    if (line_h <= 0) return;

    if (!tb->skip_clear) GfxFillRect(tile->fb, tile->box.x, tile->box.y, tile->box.w, tile->box.h, tb->BgColor);

    int n_lines = 0;
    const char *p = tb->text;
    while (*p) {
        line_info_t info = find_next_line(tb->Font, p, tile->box.w);
        n_lines++;
        p = info.next;
    }

    int visible = tile->box.h / line_h;
    if (visible < 1) visible = 1;

    int skip_lines, render_lines, y_top;

    if (n_lines * line_h <= tile->box.h) {
        skip_lines   = 0;
        render_lines = n_lines;
        switch (tb->valign) {
        case GFX_VALIGN_MIDDLE: y_top = tile->box.y + (tile->box.h - n_lines * line_h) / 2; break;
        case GFX_VALIGN_BOTTOM: y_top = tile->box.y + tile->box.h - n_lines * line_h;       break;
        case GFX_VALIGN_TOP:
        default:                y_top = tile->box.y;                                        break;
        }
    } else {
        switch (tb->valign) {
        case GFX_VALIGN_MIDDLE: skip_lines = (n_lines - visible) / 2;  break;
        case GFX_VALIGN_BOTTOM: skip_lines = n_lines - visible;        break;
        case GFX_VALIGN_TOP:
        default:                skip_lines = 0;                        break;
        }
        render_lines = visible;
        y_top        = tile->box.y;
    }

    p = tb->text;
    for (int i = 0; i < skip_lines && *p; i++) {
        line_info_t info = find_next_line(tb->Font, p, tile->box.w);
        p = info.next;
    }

    int line_y = y_top;
    for (int i = 0; i < render_lines && *p; i++) {
        line_info_t info = find_next_line(tb->Font, p, tile->box.w);

        int line_x;
        switch (tb->halign) {
        case GFX_ALIGN_CENTER: line_x = tile->box.x + (tile->box.w - info.width) / 2;  break;
        case GFX_ALIGN_RIGHT:  line_x = tile->box.x + tile->box.w - info.width;        break;
        case GFX_ALIGN_LEFT:
        default:               line_x = tile->box.x;                                   break;
        }

        for (const char *q = p; q < info.end; q++) {
            line_x = GfxDrawCharBg(tile->fb, tb->Font, line_x, line_y, (u8)*q,
                                   tb->Color, tb->BgColor);
        }
        p = info.next;
        line_y += line_h;
    }
}
