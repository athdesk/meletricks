#include "screen_life.h"
#include "widget_life.h"

GfxScreen  s_life_scr;
GfxScreen  s_life_settings_scr;
GfxWidget *s_life_settings_menu;
GfxWidget *s_life_widget;

static GfxWidgetSlot s_life_slots[4];
static GfxWidgetSlot s_life_settings_slots[3];

/* -- Life tunables ----------------------------------------------- */

static int s_life_spawn_n = 15;    /* 1-in-N preset chance / keystroke */
static int s_life_step_ms = 100;   /* generation step interval         */

int         life_spawn_get(void)  { return s_life_spawn_n; }
static void life_spawn_set(int v) { s_life_spawn_n = (v < 1) ? 1 : v; }
static int  life_step_get(void)   { return s_life_step_ms; }
static void life_step_set(int v)
{
    s_life_step_ms = (v < 100) ? 100 : v;
    if (s_life_widget)
        ((LifeDemo *)s_life_widget->data)->step_interval_ms = (u32)s_life_step_ms;
}

static const GfxMenuItem LIFE_SETTINGS_ITEMS[] = {
    { "Life 1/N", GFX_MENU_SLIDER,
      .slider = { life_spawn_get, life_spawn_set, 1, 30, 1 } },
    { "Life ms",  GFX_MENU_SLIDER,
      .slider = { life_step_get,  life_step_set, 100, 500, 25 } },
};
#define LIFE_SETTINGS_COUNT \
    ((int)(sizeof(LIFE_SETTINGS_ITEMS)/sizeof(LIFE_SETTINGS_ITEMS[0])))

static void accent_life(GfxWidget *w, GfxColor c)
{ LifeDemoSetAccent(w->data, c); GfxMarkDirty(w); }

static void bg_life(GfxWidget *w, GfxColor c)
{ LifeDemoSetBg(w->data, c);    GfxMarkDirty(w); }

void build_life(void)
{
    int cell_px = 5;
    int cols    = (LCD_W - 2 * BODY_INSET_X) / cell_px;
    int rows    = BODY_H / cell_px;
    s_life_widget = NewLifeDemo(
        .cell_px = cell_px, .cols = cols, .rows = rows,
        .color = accent_color(), .bg_color = bg_color_value(),
        .step_interval_ms = (u32)life_step_get(),
    );
    /* Heap the grid buffers separately so the compound literal stays
     * small enough for the setup-task stack. */
    LifeDemo *l = s_life_widget->data;
    u32 n = (u32)(cols * rows);
    l->grid       = GfxAlloc(n);
    l->next       = GfxAlloc(n);
    l->prev_drawn = GfxAlloc(n);
    for (u32 i = 0; i < n; i++) { l->grid[i] = 0; l->next[i] = 0; l->prev_drawn[i] = 0; }

    settings_register_accent(s_life_widget, accent_life);
    settings_register_bg(s_life_widget, bg_life);

    s_life_slots[0] = (GfxWidgetSlot){ s_life_widget, BODY_SLOT };
    s_life_slots[1] = (GfxWidgetSlot){ s_statusbar,   STATUSBAR_SLOT };
    s_life_slots[2] = (GfxWidgetSlot){ s_navbar,      NAVBAR_SLOT };
    s_life_slots[3] = (GfxWidgetSlot){ make_border(),  BORDER_SLOT };
    s_life_scr = (GfxScreen){
        .name = "Life",
        .slots = s_life_slots, .slot_count = 4,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_life_scr);

    s_life_settings_menu = make_menu(LIFE_SETTINGS_ITEMS, LIFE_SETTINGS_COUNT,
                                     GFX_MENU_INDICATOR_NONE);
    settings_register_accent(s_life_settings_menu, accent_menu);
    settings_register_bg(s_life_settings_menu, bg_menu);
    s_life_settings_slots[0] = (GfxWidgetSlot){ s_life_settings_menu, MENU_SLOT };
    s_life_settings_slots[1] = (GfxWidgetSlot){ s_navbar,             NAVBAR_SLOT };
    s_life_settings_slots[2] = (GfxWidgetSlot){ make_border(),        BORDER_SLOT };
    s_life_settings_scr = (GfxScreen){
        .name = "Life Settings",
        .slots = s_life_settings_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_life_settings_scr);
}
