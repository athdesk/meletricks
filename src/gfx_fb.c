#include "gfx_fb.h"
#include "mem.h"

// Performance: much faster than a u16 store loop
static inline void fill_u32_burst(u32 *dst, u32 n, u32 v)
{
    __asm__ volatile (
        "cmp     %[n], #8           \n"
        "blt     2f                 \n"
        "mov     r3, %[v]           \n"
        "mov     r4, %[v]           \n"
        "mov     r5, %[v]           \n"
        "mov     r6, %[v]           \n"
        "mov     r7, %[v]           \n"
        "mov     r8, %[v]           \n"
        "mov     r9, %[v]           \n"
        "mov     r10, %[v]          \n"
        "1:                         \n"
        "stmia   %[d]!, {r3-r10}    \n"
        "subs    %[n], %[n], #8     \n"
        "cmp     %[n], #8           \n"
        "bge     1b                 \n"
        "2:                         \n"
        "cmp     %[n], #0           \n"
        "beq     4f                 \n"
        "3:                         \n"
        "str     %[v], [%[d]], #4   \n"
        "subs    %[n], %[n], #1     \n"
        "bne     3b                 \n"
        "4:                         \n"
        : [d] "+r" (dst), [n] "+r" (n)
        : [v] "r" (v)
        : "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "memory", "cc"
    );
}

static void fill_color(u16 *p, u32 n_pixels, u16 c)
{
    if (n_pixels == 0) return;

    if ((u32)(unsigned long)p & 2u) {
        *p++ = c;
        n_pixels--;
        if (n_pixels == 0) return;
    }

    u32 words = n_pixels >> 1;
    if (words) {
        u32 pair = ((u32)c << 16) | c;
        fill_u32_burst((u32 *)p, words, pair);
    }
    if (n_pixels & 1u) p[n_pixels - 1] = c;
}

void GfxFillSpan(u16 *dst, int w, GfxColor c)
{
    fill_color(dst, (u32)w, c);
}

void GfxFbInit(GfxFb *fb, u16 *pixels, int w, int h)
{
    fb->pixels  = pixels;
    fb->width   = (s16)w;
    fb->height  = (s16)h;
    fb->clip.x0 = 0;
    fb->clip.y0 = 0;
    fb->clip.x1 = (s16)w;
    fb->clip.y1 = (s16)h;
}

GfxClip GfxFbPushClip(GfxFb *fb, GfxBoundingBox box)
{
    GfxClip saved = { fb->clip.x0, fb->clip.y0, fb->clip.x1, fb->clip.y1 };
    int x0 = box.x;
    int y0 = box.y;
    int x1 = box.x + box.w;
    int y1 = box.y + box.h;
    if (x0 < saved.x0) x0 = saved.x0;
    if (y0 < saved.y0) y0 = saved.y0;
    if (x1 > saved.x1) x1 = saved.x1;
    if (y1 > saved.y1) y1 = saved.y1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    fb->clip.x0 = (s16)x0;
    fb->clip.y0 = (s16)y0;
    fb->clip.x1 = (s16)x1;
    fb->clip.y1 = (s16)y1;
    return saved;
}

void GfxFbPopClip(GfxFb *fb, GfxClip saved)
{
    fb->clip.x0 = saved.x0;
    fb->clip.y0 = saved.y0;
    fb->clip.x1 = saved.x1;
    fb->clip.y1 = saved.y1;
}

/* fb_clear ignores the clip on purpose — it's a full-panel reset that
 * runs before any widget pushes its own clip. */
void GfxFbClear(GfxFb *fb, GfxColor c)
{
    fill_color(fb->pixels, (u32)fb->width * (u32)fb->height, c);
}

void GfxBlitFb(GfxFb *fb, int x, int y, const GfxFb *src)
{
    if (!src || !src->pixels) return;
    int sx = 0, sy = 0;
    int w  = src->width;
    int h  = src->height;
    if (!GfxFbClipRectSrc(fb, &x, &y, &w, &h, &sx, &sy)) return;

    u16       *dst_row = &fb->pixels [y * fb->width  + x];
    const u16 *src_row = &src->pixels[sy * src->width + sx];
    int dst_stride = fb->width;
    int src_stride = src->width;
    u32 bytes = (u32)w * 2u;
    while (h--) {
        memcpy(dst_row, src_row, bytes);
        dst_row += dst_stride;
        src_row += src_stride;
    }
}

