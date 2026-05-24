#ifndef GFX_WIDGETS_GRAPH_H
#define GFX_WIDGETS_GRAPH_H
#include "gfx.h"

/* -- Graph ---- line graph with optional autoscaling. */
typedef struct {
    GfxColor       color;
    GfxColor       bg_color;
    bool           skip_clear;
    int            line_width;
    bool           autoscale;
    /* Legend: h-lines at top / mid / bottom of the data range, labelled
     * with their values. legend_font may be NULL for lines without text. */
    bool           show_legend;
    GfxColor       legend_color;
    const GfxFont *legend_font;   /* NULL = lines only */
    int            data_min;
    int            data_max;
    const int     *data;
    int            data_count;
} GfxGraph;

void GfxGraphDraw(GfxRenderingTile *tile, GfxGraph *g);

#define NewGfxGraph(...) \
    GfxNewWidget(sizeof(GfxGraph), \
                 &(GfxGraph){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxGraphDraw), \
                 NULL)

#endif
