#include "screen_fonts.h"

GfxScreen  s_fonts_scr;
GfxWidget *s_fonts_menu;

static GfxWidgetSlot s_fonts_slots[3];

/* -- Header font (breadcrumb + battery%) ------------------------- */

static const GfxFont *const HEADER_FONTS[] = {
    &font_montserrat_14,
    &font_fira_mono_14,
    &font_lora_14,
    &font_barlow_condensed_14,
};
#define HEADER_FONT_COUNT ((int)(sizeof(HEADER_FONTS)/sizeof(HEADER_FONTS[0])))

static const char *const HEADER_FONT_LABELS[] = {
    "Montserrat", "Fira Mono", "Lora", "Barlow Cond",
};

static int s_header_font_idx = 0;
static int header_font_get(void) { return s_header_font_idx; }
static void header_font_set(int v)
{
    if (v < 0 || v >= HEADER_FONT_COUNT) return;
    s_header_font_idx = v;
    settings_apply_header_font(HEADER_FONTS[v]);
}

/* -- List font (menu items) -------------------------------------- */

static const GfxFont *const LIST_FONTS[] = {
    &font_montserrat_14,
    &font_fira_mono_14,
    &font_lora_14,
    &font_barlow_condensed_14,
};
#define LIST_FONT_COUNT ((int)(sizeof(LIST_FONTS)/sizeof(LIST_FONTS[0])))

static const char *const LIST_FONT_LABELS[] = {
    "Montserrat", "Fira Mono", "Lora", "Barlow Cond",
};

static int s_list_font_idx = 0;
static int list_font_get(void) { return s_list_font_idx; }
static void list_font_set(int v)
{
    if (v < 0 || v >= LIST_FONT_COUNT) return;
    s_list_font_idx = v;
    settings_apply_list_font(LIST_FONTS[v]);
}

/* -- Carousel font ----------------------------------------------- */

static const GfxFont *const CAROUSEL_FONTS[] = {
    &font_lora_24,
    &font_montserrat_24,
    &font_fira_mono_24,
    &font_barlow_condensed_24,
};
#define CAROUSEL_FONT_COUNT ((int)(sizeof(CAROUSEL_FONTS)/sizeof(CAROUSEL_FONTS[0])))

static const char *const CAROUSEL_FONT_LABELS[] = {
    "Lora", "Montserrat", "Fira Mono", "Barlow Cond",
};

static int s_carousel_font_idx = 0;
static int carousel_font_get(void) { return s_carousel_font_idx; }
static void carousel_font_set(int v)
{
    if (v < 0 || v >= CAROUSEL_FONT_COUNT) return;
    s_carousel_font_idx = v;
    settings_apply_carousel_font(CAROUSEL_FONTS[v]);
}

/* -- Menu items -------------------------------------------------- */

static const GfxMenuItem FONTS_ITEMS[] = {
    { "Header",   GFX_MENU_CHOICE,
      .choice = { HEADER_FONT_LABELS,   HEADER_FONT_COUNT,
                  header_font_get,   header_font_set } },
    { "List",     GFX_MENU_CHOICE,
      .choice = { LIST_FONT_LABELS,     LIST_FONT_COUNT,
                  list_font_get,     list_font_set } },
    { "Carousel", GFX_MENU_CHOICE,
      .choice = { CAROUSEL_FONT_LABELS, CAROUSEL_FONT_COUNT,
                  carousel_font_get, carousel_font_set } },
};
#define FONTS_COUNT ((int)(sizeof(FONTS_ITEMS)/sizeof(FONTS_ITEMS[0])))

void build_fonts_screen(void)
{
    s_fonts_menu = make_menu(FONTS_ITEMS, FONTS_COUNT, GFX_MENU_INDICATOR_LEFT);
    settings_register_accent(s_fonts_menu, accent_menu);
    settings_register_bg(s_fonts_menu, bg_menu);
    s_fonts_slots[0] = (GfxWidgetSlot){ s_fonts_menu, MENU_SLOT };
    s_fonts_slots[1] = (GfxWidgetSlot){ s_navbar,     NAVBAR_SLOT };
    s_fonts_slots[2] = (GfxWidgetSlot){ make_border(), BORDER_SLOT };
    s_fonts_scr = (GfxScreen){
        .name = "Fonts",
        .slots = s_fonts_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_fonts_scr);
}
