#ifndef GFX_WIDGETS_CAROUSEL_H
#define GFX_WIDGETS_CAROUSEL_H
#include "gfx.h"
#include "gfx_icon.h"

typedef struct {
    const char *label;
    GfxIcon     icon;
} GfxCarouselItem;

typedef struct {
    int                    w, h;          /* physics dimensions       */
    GfxColor               color, bg_color;
    const GfxFont         *font;          /* label font               */
    const GfxCarouselItem *items;
    int                    item_count;
    int                    selected;
    void                 (*on_select)(GfxCarouselItem *item);
    int                    anim_speed;    /* px/tick; 0 = instant     */
    /* private */
    int                    _anim_offset;  /* current render offset    */
    int                    _cached_in_w;  /* selected icon width      */
    int                    _cached_out_w; /* outgoing icon width      */
    int                    _cached_slide; /* total slide distance     */
    bool                   skip_clear;
} GfxCarousel;

void GfxCarouselDraw  (GfxRenderingTile *tile, GfxCarousel *c);
int  GfxCarouselTick  (void *data);   /* void* for GfxWidget tick compat */
void GfxCarouselNext  (GfxCarousel *c);
void GfxCarouselPrev  (GfxCarousel *c);
void GfxCarouselSelect(GfxCarousel *c);

#define NewGfxCarousel(...) \
    GfxNewWidget(sizeof(GfxCarousel), \
                 &(GfxCarousel){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(GfxCarouselDraw), \
                 GFX_TICK_FN(GfxCarouselTick))

#endif /* GFX_WIDGETS_CAROUSEL_H */
