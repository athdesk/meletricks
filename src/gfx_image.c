#include "gfx.h"
#include "mem.h"

void GfxBlitImage(GfxFb *fb, int x, int y, int w, int h, const u16 *pixels)
{
    int sx = 0, sy = 0;
    int src_w = w;
    if (!GfxFbClipRectSrc(fb, &x, &y, &w, &h, &sx, &sy)) return;

    u16       *dst_row = &fb->pixels[y * fb->width + x];
    const u16 *src_row = pixels + sy * src_w + sx;

    u32 bytes = (u32)w * 2u;
    while (h--) {
        memcpy(dst_row, src_row, bytes);
        dst_row += fb->width;
        src_row += src_w;
    }
}

/* Keyed blit — pixels matching transparent_key are skipped. */
void GfxDrawImage(GfxFb *fb, int x, int y, int w, int h,
                  const u16 *pixels, GfxColor transparent_key)
{
    if (w <= 0 || h <= 0) return;

    int sx = 0, sy = 0;
    int src_w = w;
    if (!GfxFbClipRectSrc(fb, &x, &y, &w, &h, &sx, &sy)) return;

    u16       *dst_row = &fb->pixels[y * fb->width + x];
    const u16 *src_row = pixels + sy * src_w + sx;

    for (int row = 0; row < h; row++) {
        u16       *dp = dst_row;
        const u16 *sp = src_row;
        for (int col = 0; col < w; col++) {
            u16 px = *sp++;
            if (px != transparent_key) *dp = px;
            dp++;
        }
        dst_row += fb->width;
        src_row += src_w;
    }
}

void GfxDrawAnim(GfxFb *fb, int x, int y, int w, int h,
                 const u16 *frames_pixels, int frame_count,
                 int frame_index, GfxColor transparent_key)
{
    if (frame_count <= 0) return;
    int idx = frame_index % frame_count;
    if (idx < 0) idx += frame_count;
    const u16 *frame = frames_pixels + (idx * w * h);
    GfxDrawImage(fb, x, y, w, h, frame, transparent_key);
}
