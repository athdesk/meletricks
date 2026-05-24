#ifndef GFX_WIDGETS_TEXTBOX_H
#define GFX_WIDGETS_TEXTBOX_H
#include "gfx.h"

/* -- Textbox ---- multi-line text with align + word-wrap.
 *
 * Background clear: by default the widget fills (x,y,w,h) with
 * bg_color before drawing the text. Set `skip_clear` to stack the
 * textbox on top of another widget (e.g. a label over an image). */
typedef struct {
    const GfxFont *font;
    GfxColor       color;
    GfxColor       bg_color;
    bool           skip_clear;
    const char    *text;
    GfxHalign      halign;
    GfxValign      valign;
} GfxTextbox;

void GfxTextboxDraw(GfxRenderingTile *tile, GfxTextbox *t);

#define NewGfxTextbox(...) \
    GfxNewWidget(sizeof(GfxTextbox), \
                 &(GfxTextbox){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxTextboxDraw), \
                 NULL)

#endif
