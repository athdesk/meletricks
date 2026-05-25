#ifndef GFX_H
#define GFX_H

#include "gfx_font.h"
#include "gfx_fb.h"
#include "gfx_screen.h"
#include "gfx_shapes.h"

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

typedef enum {
    GFX_ALIGN_LEFT   = 0,
    GFX_ALIGN_CENTER = 1,
    GFX_ALIGN_RIGHT  = 2,
} GfxHalign;

typedef enum {
    GFX_VALIGN_TOP    = 0,
    GFX_VALIGN_MIDDLE = 1,
    GFX_VALIGN_BOTTOM = 2,
} GfxValign;

/* Opaque blit — no transparency check; uses memcpy per row. */
void GfxBlitImage(GfxFb *fb, int x, int y, int w, int h, const u16 *pixels);

/* Keyed blit — pixels matching transparent_key are skipped. */
void GfxDrawImage(GfxFb *fb, int x, int y, int w, int h,
                  const u16 *pixels, GfxColor transparent_key);
void GfxDrawAnim (GfxFb *fb, int x, int y, int w, int h,
                  const u16 *frames_pixels, int frame_count,
                  int frame_index, GfxColor transparent_key);

/* Fill the entire tile area with a solid colour. */
static inline void GfxFillTile(GfxRenderingTile *tile, GfxColor c)
{
    GfxFillRect(tile->fb, tile->box.x, tile->box.y, tile->box.w, tile->box.h, c);
}

/* -- Widget headers ----------------------------------------------- */
#include "widgets/border.h"
#include "widgets/textbox.h"
#include "widgets/menu_list.h"
#include "widgets/clock.h"
#include "widgets/graph.h"
#include "widgets/progress.h"
#include "widgets/marquee.h"
#include "widgets/breadcrumb.h"
#include "gfx_icon.h"
#include "widgets/carousel.h"

#endif
