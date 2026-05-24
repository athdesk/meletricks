#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "fr_types.h"

#define GFX_FONT_FLAG_MONOSPACE  0x01u
#define GFX_FONT_FLAG_MSB_FIRST  0x02u
/* Renderers that don't know about this flag will misinterpret the
 * bitmap as 1-bpp — use GfxDrawCharBg / GfxDrawTextBg to pick the
 * right path automatically. */
#define GFX_FONT_FLAG_2BPP       0x04u

#define GFX_GLYPH_ABSENT  0xFFu

typedef struct {
    u16 bitmap_off;
    u8  width;
    u8  height;
    s8  x_off;
    s8  y_off;
    u8  x_advance;
    u8  _reserved;
} GfxGlyph;

typedef struct {
    u8  line_height;
    u8  baseline;
    u8  flags;
    u8  default_advance;
    u8  first_cp;
    u8  last_cp;
    u16 bitmap_bytes;
    const u8          *index;
    const GfxGlyph *glyphs;
    const u8          *bitmap;
} GfxFont;

#endif
