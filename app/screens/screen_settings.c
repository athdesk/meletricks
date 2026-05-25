#include "screen_settings.h"
#include "screen_fonts.h"
#include "screen_text.h"

GfxScreen  s_settings_scr;
GfxScreen  s_debug_scr;
GfxScreen  s_about_scr;
GfxWidget *s_settings_menu;
GfxWidget *s_debug_menu;

static GfxWidgetSlot s_settings_slots[3];
static GfxWidgetSlot s_debug_slots[3];
static GfxWidgetSlot s_about_slots[3];

static const GfxMenuItem SETTINGS_ITEMS[] = {
    { "About",            GFX_MENU_LINK,
      .link   = { &s_about_scr } },
    { "Debug",            GFX_MENU_LINK,
      .link   = { &s_debug_scr } },
    { "Fonts",            GFX_MENU_LINK,
      .link   = { &s_fonts_scr } },
    { "Invert input",     GFX_MENU_TOGGLE,
      .toggle = { input_inverted_get, input_inverted_set } },
    { "Clock seconds",    GFX_MENU_TOGGLE,
      .toggle = { clock_seconds_get, clock_seconds_set } },
    { "Sleep",            GFX_MENU_CHOICE,
      .choice = { SLEEP_TIMEOUT_LABELS, SLEEP_TIMEOUT_COUNT,
                  sleep_timeout_idx_get, sleep_timeout_idx_set } },
    { "Frame",            GFX_MENU_CHOICE,
      .choice = { FRAME_COLOR_LABELS, FRAME_COLOR_COUNT,
                  frame_color_get,    frame_color_set } },
    { "Foreground",       GFX_MENU_CHOICE,
      .choice = { FG_COLOR_LABELS, FG_COLOR_COUNT,
                  fg_color_get,    fg_color_set } },
    { "Secondary",        GFX_MENU_CHOICE,
      .choice = { SECONDARY_COLOR_LABELS, SECONDARY_COLOR_COUNT,
                  secondary_color_get, secondary_color_set } },
    { "Background",       GFX_MENU_CHOICE,
      .choice = { BG_COLOR_LABELS, BG_COLOR_COUNT,
                  bg_color_get,    bg_color_set } },
};
#define SETTINGS_COUNT ((int)(sizeof(SETTINGS_ITEMS)/sizeof(SETTINGS_ITEMS[0])))

static const GfxMenuItem DEBUG_ITEMS[] = {
    { "Fw Override", GFX_MENU_TOGGLE,
      .toggle = { sdk_get_fw_override, sdk_set_fw_override } },
    { "Show FPS",   GFX_MENU_TOGGLE,
      .toggle = { fps_show_get,     fps_show_set } },
    { "Debug Pink", GFX_MENU_TOGGLE,
      .toggle = { GfxGetDebugFill,    GfxSetDebugFill } },
    { "Idle gate",  GFX_MENU_TOGGLE,
      .toggle = { GfxGetTickGateIdle, GfxSetTickGateIdle } },
    { "Always present", GFX_MENU_TOGGLE,
      .toggle = { GfxGetAlwaysPresent, GfxSetAlwaysPresent } },
    { "Key events", GFX_MENU_LINK,
      .link   = { &s_text_scr } },
    { "Sleep now",  GFX_MENU_ACTION,
      .action = { meletricks_sleep_now } },
};
#define DEBUG_COUNT ((int)(sizeof(DEBUG_ITEMS)/sizeof(DEBUG_ITEMS[0])))

static void build_static_leaf(GfxWidgetSlot *slots, GfxScreen *scr,
                               const char *name, const char *body_text)
{
    slots[0] = (GfxWidgetSlot){ make_placeholder(body_text), BODY_SLOT };
    slots[1] = (GfxWidgetSlot){ s_navbar,                    NAVBAR_SLOT };
    slots[2] = (GfxWidgetSlot){ make_border(),               BORDER_SLOT };
    *scr = (GfxScreen){
        .name = name,
        .slots = slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(scr);
}

void build_settings_screens(void)
{
    s_settings_menu = make_menu(SETTINGS_ITEMS, SETTINGS_COUNT,
                                GFX_MENU_INDICATOR_LEFT);
    settings_register_accent(s_settings_menu, accent_menu);
    settings_register_bg(s_settings_menu, bg_menu);
    s_settings_slots[0] = (GfxWidgetSlot){ s_settings_menu, MENU_SLOT };
    s_settings_slots[1] = (GfxWidgetSlot){ s_navbar,        NAVBAR_SLOT };
    s_settings_slots[2] = (GfxWidgetSlot){ make_border(),   BORDER_SLOT };
    s_settings_scr = (GfxScreen){
        .name = "Settings",
        .slots = s_settings_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_settings_scr);

    /* Debug sub-screen — Realtime toggle plus on-screen instrumentation
     * (FPS overlay, debug-pink paint mode). Same chrome as Settings. */
    s_debug_menu = make_menu(DEBUG_ITEMS, DEBUG_COUNT, GFX_MENU_INDICATOR_NONE);
    settings_register_accent(s_debug_menu, accent_menu);
    settings_register_bg(s_debug_menu, bg_menu);
    s_debug_slots[0] = (GfxWidgetSlot){ s_debug_menu, MENU_SLOT };
    s_debug_slots[1] = (GfxWidgetSlot){ s_navbar,     NAVBAR_SLOT };
    s_debug_slots[2] = (GfxWidgetSlot){ make_border(), BORDER_SLOT };
    s_debug_scr = (GfxScreen){
        .name = "Debug",
        .slots = s_debug_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_debug_scr);

    build_static_leaf(s_about_slots, &s_about_scr,
                      "About", "Meletricks v0\nMade with <3 by hdesk");
}
