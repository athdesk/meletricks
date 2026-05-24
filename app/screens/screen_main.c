#include "screen_main.h"
#include "screen_bounce.h"
#include "screen_wireframe.h"
#include "screen_life.h"
#include "screen_clock.h"
#include "screen_graph.h"
#include "screen_settings.h"
#include "../icons.h"

GfxScreen  s_main;
GfxWidget *s_main_carousel;

static GfxWidgetSlot s_main_slots[4];

static const GfxCarouselItem CAROUSEL_ITEMS[] = {
    { "Bounce",    NewGfxIconProportional(icon_anim,     NULL) },
    { "Wireframe", NewGfxIconProportional(icon_cube,     NULL) },
    { "Life",      NewGfxIconProportional(icon_life,     NULL) },
    { "Clock",     NewGfxIconProportional(icon_clock, NULL) },
    { "WPM Graph", NewGfxIconProportional(icon_graph,    NULL) },
    { "Settings",  NewGfxIconProportional(icon_settings, NULL) },
};
#define CAROUSEL_COUNT ((int)(sizeof(CAROUSEL_ITEMS)/sizeof(CAROUSEL_ITEMS[0])))

/* Parallel screen table — indexed by CAROUSEL_ITEMS position. */
static GfxScreen *const s_carousel_screens[] = {
    &s_bounce_scr, &s_wireframe_scr, &s_life_scr,
    &s_clock_scr, &s_graph_scr, &s_settings_scr,
};

static void on_carousel_select(GfxCarouselItem *item)
{
    int idx = (int)(item - CAROUSEL_ITEMS);
    if ((unsigned)idx < CAROUSEL_COUNT)
        GfxNavTo(s_carousel_screens[idx]);
}

static void bg_carousel(GfxWidget *w, GfxColor c)
{ ((GfxCarousel *)w->data)->bg_color = c; GfxMarkDirty(w); }

static void font_carousel(GfxWidget *w, const GfxFont *f)
{ ((GfxCarousel *)w->data)->font = f; GfxMarkDirty(w); }

void build_main(void)
{
    s_main_carousel = NewGfxCarousel(
        .w = LCD_W, .h = BODY_H,
        .font      = &font_lora_24,
        .color     = GFX_WHITE,
        .bg_color  = bg_color_value(),
        .items     = CAROUSEL_ITEMS,
        .item_count = CAROUSEL_COUNT,
        .on_select = on_carousel_select,
        .anim_speed = 18,
    );
    settings_register_bg(s_main_carousel, bg_carousel);
    settings_register_carousel_font(s_main_carousel, font_carousel);
    s_main_slots[0] = (GfxWidgetSlot){ s_main_carousel, CAROUSEL_SLOT };
    s_main_slots[1] = (GfxWidgetSlot){ s_statusbar,     STATUSBAR_SLOT };
    s_main_slots[2] = (GfxWidgetSlot){ s_navbar,        NAVBAR_SLOT };
    s_main_slots[3] = (GfxWidgetSlot){ make_border(),    BORDER_SLOT };
    s_main = (GfxScreen){
        .name = "Main",
        .slots = s_main_slots, .slot_count = 4,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_main);
}
