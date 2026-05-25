#ifndef ICONS_H
#define ICONS_H

#include "gfx.h"

void icon_life    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_cube    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_anim    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_text    (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_graph   (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_settings(GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);
void icon_clock   (GfxFb *fb, int x, int y, int h, GfxColor fg, GfxColor bg, void *data);

#endif
