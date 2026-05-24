/* LifeDemo: Conway's Life on a `cols x rows` grid of `cell_px` cells.
 * Toroidal topology (both axes wrap). Grid arrays are external heap
 * allocations — embedding them would push NewLifeDemo's compound
 * literal (~4.6 KB) over the stack budget. */
#ifndef HELLO_WIDGET_LIFE_H
#define HELLO_WIDGET_LIFE_H

#include "gfx.h"

typedef struct {
    int      cell_px;
    int      cols, rows;
    GfxColor color;
    GfxColor bg_color;
    u32      step_interval_ms;
    /* internal — caller allocs after construction (see hello_blob.c). */
    u8      *grid;
    u8      *next;
    u8      *prev_drawn;
    u32      last_step_ms;
} LifeDemo;

void LifeDemoDraw(GfxRenderingTile *tile, LifeDemo *l);
int  LifeDemoTick(LifeDemo *l);

void LifeDemoAddRandomPattern(LifeDemo *l);

/* Recolour and force a full repaint: invalidates the prev-cell cache
 * so every live cell repaints in the new colour (otherwise unchanged
 * cells stick to the old colour until their state flips). */
void LifeDemoSetAccent(LifeDemo *l, GfxColor c);

/* Same shape for the bg colour — dead-cell background changes, so the
 * cache has to be invalidated to repaint the dead cells too. */
void LifeDemoSetBg(LifeDemo *l, GfxColor c);

#define NewLifeDemo(...) \
    GfxNewWidget(sizeof(LifeDemo), &(LifeDemo){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(LifeDemoDraw), GFX_TICK_FN(LifeDemoTick))

#endif
