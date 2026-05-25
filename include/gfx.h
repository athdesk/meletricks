#ifndef GFX_H
#define GFX_H

#include "gfx_font.h"
#include "gfx_fb.h"
#include "gfx_screen.h"
#include "gfx_shapes.h"

/* -- Allocators -------------------------------------------------------
 *
 * Defaults to ke_malloc on pool 0. We may want to change this to our own allocator */
#ifndef GfxAlloc
void *ke_malloc(u32 size, u8 type);
void  ke_free(void *ptr);
#define GfxAlloc(sz)  ke_malloc((u32)(sz), 0)
#define GfxFree(p)    ke_free(p)
#endif

int  GfxDrawChar (GfxFb *fb, const GfxFont *f, int x, int y, u8 cp, GfxColor color);
int  GfxDrawText (GfxFb *fb, const GfxFont *f, int x, int y, const char *s, GfxColor color);
int  GfxTextWidth(const GfxFont *f, const char *s);
const GfxGlyph *GfxFontLookup(const GfxFont *f, u8 cp);

/* Bg-aware variants: Needed for 2bpp (alpha).
 * Caller is responsible for having filled the destination with `bg`
 * the 2-bpp path skips transparent source pixels rather than
 * touching the destination. */
int GfxDrawCharBg(GfxFb *fb, const GfxFont *f, int x, int y, u8 cp,
                  GfxColor fg, GfxColor bg);
int GfxDrawTextBg(GfxFb *fb, const GfxFont *f, int x, int y, const char *s,
                  GfxColor fg, GfxColor bg);

// TODO: Pixel background aware Text rendering functions

/* Opaque blit — no transparency check; uses memcpy per row. */
void GfxBlitImage(GfxFb *fb, int x, int y, int w, int h, const u16 *pixels);

/* Keyed blit — pixels matching transparent_key are skipped. */
void GfxDrawImage(GfxFb *fb, int x, int y, int w, int h,
                  const u16 *pixels, GfxColor transparent_key);
void GfxDrawAnim (GfxFb *fb, int x, int y, int w, int h,
                  const u16 *frames_pixels, int frame_count,
                  int frame_index, GfxColor transparent_key);

#endif
