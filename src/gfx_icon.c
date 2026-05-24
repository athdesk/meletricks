#include "gfx.h"

void GfxIconTextStaticFill(char *buf, int size, void *data)
{
    const char *s = (const char *)data;
    int i = 0;
    while (i < size - 1 && s[i]) { buf[i] = s[i]; i++; }
    buf[i] = '\0';
}

/* Measure text width up to the first '\0' or '\n'. */
static int text_icon_width(const GfxFont *font, const char *s)
{
    int w = 0;
    while (*s && *s != '\n') {
        const GfxGlyph *g = GfxFontLookup(font, (u8)*s++);
        w += g ? g->x_advance : font->default_advance;
    }
    return w;
}

int GfxIconWidth(const GfxIcon *icon, int h)
{
    if (!icon || h <= 0) return h;
    switch (icon->type) {
    case GFX_ICON_PROPORTIONAL:
        return h;
    case GFX_ICON_FIXED:
        if (icon->fixed.h <= 0) return h;
        return icon->fixed.w * h / icon->fixed.h;
    case GFX_ICON_TEXT: {
        if (!icon->text.font || !icon->text.get_text) return h;
        char buf[64];
        buf[0] = '\0';
        icon->text.get_text(buf, (int)sizeof(buf), icon->text.data);
        return text_icon_width(icon->text.font, buf);
    }
    }
    return h;
}

void GfxIconDraw(GfxFb *fb, const GfxIcon *icon,
                 int x, int y, int h,
                 GfxColor fg, GfxColor bg)
{
    if (!icon) return;

    switch (icon->type) {

    case GFX_ICON_PROPORTIONAL:
        if (icon->proportional.draw)
            icon->proportional.draw(fb, x, y, h, fg, bg, icon->proportional.data);
        break;

    case GFX_ICON_FIXED:
        if (icon->fixed.draw)
            icon->fixed.draw(fb, x, y, h, fg, bg, icon->fixed.data);
        break;

    case GFX_ICON_TEXT: {
        if (!icon->text.font || !icon->text.get_text) break;
        char buf[64];
        buf[0] = '\0';
        icon->text.get_text(buf, (int)sizeof(buf), icon->text.data);
        int th = (int)icon->text.font->line_height;
        int ty = y + (h - th) / 2;
        GfxDrawTextBg(fb, icon->text.font, x, ty, buf, fg, bg);
        break;
    }
    }
}
