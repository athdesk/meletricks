#ifndef GFX_COLOR_H
#define GFX_COLOR_H

#include "fr_types.h"

typedef u16 GfxColor;

static inline GfxColor GfxRgb(u8 r, u8 g, u8 b)
{
    return (GfxColor)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#define GFX_BLACK    ((GfxColor)0x0000)
#define GFX_WHITE    ((GfxColor)0xFFFF)
#define GFX_RED      ((GfxColor)0xF800)
#define GFX_GREEN    ((GfxColor)0x07E0)
#define GFX_BLUE     ((GfxColor)0x001F)
#define GFX_YELLOW   ((GfxColor)0xFFE0)
#define GFX_CYAN     ((GfxColor)0x07FF)
#define GFX_MAGENTA  ((GfxColor)0xF81F)
#define GFX_GREY     ((GfxColor)0x8410)

#endif
