/* Small icon drawers consumed by the main carousel. Each draws into an
 * h×h square at (x,y) in the given colour. */
#ifndef HELLO_ICONS_H
#define HELLO_ICONS_H

#include "gfx.h"

void icon_life    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_cube    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_anim    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_text    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_graph   (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_settings(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_clock   (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);

#endif
