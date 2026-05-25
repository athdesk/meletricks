#include "screen_text.h"
#include "../text_log.h"

GfxScreen  s_text_scr;
GfxWidget *s_text_widget;

static GfxWidgetSlot s_text_slots[3];

void build_text(void)
{
    /* No statusbar on this sub-screen, so the textbox fills the
     * entire no-statusbar area down to the frame margin. */
    s_text_widget = NewGfxTextbox(
        .Font   = &font_montserrat_14,
        .Color  = GFX_WHITE,
        .BgColor = bg_color_value(),
        .text   = text_buf,
        .halign = GFX_ALIGN_LEFT,
        .valign = GFX_VALIGN_BOTTOM,
    );
    settings_register_bg(s_text_widget, GFX_APPLIER_FN(GfxTextbox, BgColor));
    s_text_slots[0] = (GfxWidgetSlot){ s_text_widget,
                       { BODY_INSET_X, BODY_TOP_Y,
                         LCD_W-2*BODY_INSET_X, LCD_H-BODY_TOP_Y-4 } };
    s_text_slots[1] = (GfxWidgetSlot){ s_navbar,      NAVBAR_SLOT };
    s_text_slots[2] = (GfxWidgetSlot){ make_border(),  BORDER_SLOT };
    s_text_scr = (GfxScreen){
        .name = "Key events",
        .slots = s_text_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_text_scr);
}
