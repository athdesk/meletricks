#include "gfx.h"

#define BREADCRUMB_MAX_DEPTH 8

static u16 blend565(u16 src, u16 bg, u8 alpha)
{
    u32 ia = 255u - alpha;
    u32 sr = (src >> 11) & 0x1Fu;
    u32 sg = (src >> 5)  & 0x3Fu;
    u32 sb =  src        & 0x1Fu;
    u32 br = (bg  >> 11) & 0x1Fu;
    u32 bg6= (bg  >> 5)  & 0x3Fu;
    u32 bb =  bg         & 0x1Fu;
    u32 r = (sr * alpha + br  * ia + 128u) >> 8;
    u32 g = (sg * alpha + bg6 * ia + 128u) >> 8;
    u32 b = (sb * alpha + bb  * ia + 128u) >> 8;
    return (u16)((r << 11) | (g << 5) | b);
}

// Need this to fade left when overflowing
static void apply_fade(GfxFb *fb, int x, int y, int fade_w, int h, GfxColor bg)
{
    if (fade_w <= 0 || h <= 0) return;
    int x0 = x, y0 = y, x1 = x + fade_w, y1 = y + h;
    if (!GfxFbClipBox(fb, &x0, &y0, &x1, &y1)) return;

    for (int col = x0; col < x1; col++) {
        u32 a = (u32)(col - x + 1) * 255u / (u32)fade_w;
        if (a > 255u) a = 255u;
        u8 alpha = (u8)a;
        u16 *row = &fb->pixels[y0 * fb->width + col];
        for (int yy = y0; yy < y1; yy++) {
            *row = blend565(*row, bg, alpha);
            row += fb->width;
        }
    }
}

void GfxBreadcrumbDraw(GfxRenderingTile *tile, GfxBreadcrumb *bc)
{
    if (!bc || !bc->Font || tile->box.w <= 0) return;
    int h = (tile->box.h > 0) ? tile->box.h : bc->Font->line_height;

    if (!bc->skip_clear) GfxFillRect(tile->fb, tile->box.x, tile->box.y, tile->box.w, h, bc->BgColor);

    const char *labels[BREADCRUMB_MAX_DEPTH];
    int n = 0;
    GfxScreen *s = GfxCurrentScreen();
    while (s) { labels[n++ % BREADCRUMB_MAX_DEPTH] = s->name; s = s->parent; }
    for (int i = 0, j = n - 1; i < j; i++, j--) {
        const char *t = labels[i]; labels[i] = labels[j]; labels[j] = t;
    }
    if (n <= 0) return;

    const char *sep   = bc->separator ? bc->separator : " > ";
    int         sep_w = GfxTextWidth(bc->Font, sep);
    int total_w = 0;
    for (int i = 0; i < n; i++) {
        if (i > 0) total_w += sep_w;
        total_w += GfxTextWidth(bc->Font, labels[i]);
    }

    GfxClip saved = GfxFbPushClip(tile->fb, (GfxBoundingBox){tile->box.x, tile->box.y, tile->box.w, h});

    if (total_w <= tile->box.w) {
        int px = tile->box.x;
        for (int i = 0; i < n; i++) {
            if (i > 0) px = GfxDrawTextBg(tile->fb, bc->Font, px, tile->box.y, sep, bc->Color, bc->BgColor);
            px = GfxDrawTextBg(tile->fb, bc->Font, px, tile->box.y, labels[i], bc->Color, bc->BgColor);
        }
        GfxFbPopClip(tile->fb, saved);
        return;
    }

    int px = tile->box.x + tile->box.w - total_w;
    for (int i = 0; i < n; i++) {
        if (i > 0) px = GfxDrawTextBg(tile->fb, bc->Font, px, tile->box.y, sep, bc->Color, bc->BgColor);
        px = GfxDrawTextBg(tile->fb, bc->Font, px, tile->box.y, labels[i], bc->Color, bc->BgColor);
    }
    apply_fade(tile->fb, tile->box.x, tile->box.y, bc->fade_width, h, bc->BgColor);

    GfxFbPopClip(tile->fb, saved);
}
