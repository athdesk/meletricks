#include "gfx_fb.h"

/* Only set bits (=1) write to the framebuffer; clear bits are
 * a no-op (opaque foreground over preserved background — caller fills
 * first if a background is wanted). */
void GfxBlit1Bpp(GfxFb *fb,
                   int x, int y, int w, int h,
                   const u8 *src, int src_stride_bytes,
                   GfxColor color)
{
    if (w <= 0 || h <= 0) return;

    int sx = 0, sy = 0;
    if (!GfxFbClipRectSrc(fb, &x, &y, &w, &h, &sx, &sy)) return;

    u16 *dst_row = &fb->pixels[y * fb->width + x];
    const u8 *src_row = src + sy * src_stride_bytes;

    for (int row = 0; row < h; row++) {
        const u8 *sp = src_row + (sx >> 3);
        u8  cur  = *sp++;
        int bit  = 7 - (sx & 7);
        u16 *dp  = dst_row;

        for (int col = 0; col < w; col++) {
            /* Whole-byte skip:
             * Big glyphs (consolas_64 etc.) have plenty of empty bytes
             * Huge speedup for e.g. clock */
            if (bit == 7 && cur == 0 && col + 8 <= w) {
                dp  += 8;
                col += 7;          /* loop's ++ takes the 8th step */
                cur  = *sp++;
                continue;
            }
            if (cur & (1u << bit)) *dp = color;
            dp++;
            if (bit == 0) {
                bit = 7;
                cur = *sp++;
            } else {
                bit--;
            }
        }

        dst_row += fb->width;
        src_row += src_stride_bytes;
    }
}

/* Needed to build palettes for 2bpp fonts */
static u16 blend_rgb565(u16 fg, u16 bg, int num, int den)
{
    int inv = den - num;
    int fr = (fg >> 11) & 0x1F, fg5 = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    int br = (bg >> 11) & 0x1F, bg5 = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    int r = (fr * num + br * inv) / den;
    int g = (fg5 * num + bg5 * inv) / den;
    int b = (fb * num + bb * inv) / den;
    return (u16)((r << 11) | (g << 5) | b);
}

void GfxBlit2Bpp(GfxFb *fb,
                 int x, int y, int w, int h,
                 const u8 *src, int src_stride_bytes,
                 GfxColor fg, GfxColor bg)
{
    if (w <= 0 || h <= 0) return;


    int sx = 0, sy = 0;
    if (!GfxFbClipRectSrc(fb, &x, &y, &w, &h, &sx, &sy)) return;

    u16 palette[4];
    palette[0] = 0;                            /* unused — level 0 skipped */
    palette[1] = blend_rgb565(fg, bg, 1, 3);
    palette[2] = blend_rgb565(fg, bg, 2, 3);
    palette[3] = (u16)fg;

    u16       *dst_row = &fb->pixels[y * fb->width + x];
    const u8  *src_row = src + sy * src_stride_bytes;

    for (int row = 0; row < h; row++) {
        const u8 *sp   = src_row + (sx >> 2);
        u8        cur  = *sp++;
        int       shift = 6 - ((sx & 3) << 1);
        u16      *dp   = dst_row;

        for (int col = 0; col < w; col++) {
            int level = (cur >> shift) & 3;
            if (level) *dp = palette[level];
            dp++;
            if (shift == 0) {
                shift = 6;
                cur   = *sp++;
            } else {
                shift -= 2;
            }
        }

        dst_row += fb->width;
        src_row += src_stride_bytes;
    }
}
