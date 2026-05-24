#include "screen_clock.h"

GfxScreen s_clock_scr;

static GfxWidgetSlot s_clock_slots[4];

void build_clock(void)
{
    GfxWidget *body = NewGfxClock(
        .font  = &font_montserrat_64,
        .color = GFX_WHITE,
        .bg_color = bg_color_value(),
        .show_seconds = clock_seconds_get(),
        .halign = GFX_ALIGN_CENTER, .valign = GFX_VALIGN_MIDDLE,
    );
    settings_register_bg(body, bg_clock);
    settings_register_clock(body);
    s_clock_slots[0] = (GfxWidgetSlot){ body,          BODY_SLOT };
    s_clock_slots[1] = (GfxWidgetSlot){ s_statusbar,   STATUSBAR_SLOT };
    s_clock_slots[2] = (GfxWidgetSlot){ s_navbar,      NAVBAR_SLOT };
    s_clock_slots[3] = (GfxWidgetSlot){ make_border(),  BORDER_SLOT };
    s_clock_scr = (GfxScreen){
        .name = "Clock",
        .slots = s_clock_slots, .slot_count = 4,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_clock_scr);
}
