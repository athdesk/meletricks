#include "screen_bounce.h"
#include "widget_anim.h"

GfxScreen  s_bounce_scr;
GfxScreen  s_bounce_settings_scr;
GfxWidget *s_bounce_settings_menu;

static GfxWidgetSlot s_bounce_slots[4];
static GfxWidgetSlot s_bounce_settings_slots[3];

static const GfxMenuItem BOUNCE_SETTINGS_ITEMS[] = {
    { "Clock bg", GFX_MENU_TOGGLE,
      .toggle = { demo_clock_bg_get, demo_clock_bg_set } },
};
#define BOUNCE_SETTINGS_COUNT \
    ((int)(sizeof(BOUNCE_SETTINGS_ITEMS)/sizeof(BOUNCE_SETTINGS_ITEMS[0])))

static void accent_anim(GfxWidget *w, GfxColor c)
{ ((AnimDemo *)w->data)->color = c;    GfxMarkDirty(w); }

static void bg_anim(GfxWidget *w, GfxColor c)
{ ((AnimDemo *)w->data)->bg_color = c; GfxMarkDirty(w); }

static void apply_bg_target(GfxWidget *w, const GfxRenderTarget *t)
{ ((AnimDemo *)w->data)->bg = t;       GfxMarkDirty(w); }

void build_bounce(void)
{
    GfxWidget *body = NewAnimDemo(
        .w = LCD_W - 2 * BODY_INSET_X, .h = EXPANDED_BODY_H,
        .color = accent_color(), .bg_color = bg_color_value(),
        .bg = &s_overlay_bg_target,
    );
    GfxAddWidgetDep(body, &s_overlay_bg_target);
    settings_register_accent(body, accent_anim);
    settings_register_bg(body, bg_anim);
    settings_register_bg_consumer(body, apply_bg_target);
    s_bounce_slots[0] = (GfxWidgetSlot){ body,         EXPANDED_BODY_SLOT };
    s_bounce_slots[1] = (GfxWidgetSlot){ s_statusbar,  STATUSBAR_SLOT };
    s_bounce_slots[2] = (GfxWidgetSlot){ s_navbar,     NAVBAR_SLOT };
    s_bounce_slots[3] = (GfxWidgetSlot){ make_border(), BORDER_SLOT };
    s_bounce_scr = (GfxScreen){
        .name = "Bounce",
        .slots = s_bounce_slots, .slot_count = 4,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_bounce_scr);

    s_bounce_settings_menu = make_menu(BOUNCE_SETTINGS_ITEMS, BOUNCE_SETTINGS_COUNT,
                                       GFX_MENU_INDICATOR_NONE);
    settings_register_accent(s_bounce_settings_menu, accent_menu);
    settings_register_bg(s_bounce_settings_menu, bg_menu);
    s_bounce_settings_slots[0] = (GfxWidgetSlot){ s_bounce_settings_menu, MENU_SLOT };
    s_bounce_settings_slots[1] = (GfxWidgetSlot){ s_navbar,               NAVBAR_SLOT };
    s_bounce_settings_slots[2] = (GfxWidgetSlot){ make_border(),          BORDER_SLOT };
    s_bounce_settings_scr = (GfxScreen){
        .name = "Bounce Settings",
        .slots = s_bounce_settings_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_bounce_settings_scr);
}
