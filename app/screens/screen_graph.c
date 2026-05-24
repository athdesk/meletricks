#include "screen_graph.h"
#include "../wpm.h"

GfxScreen  s_graph_scr;
GfxScreen  s_graph_settings_scr;
GfxWidget *s_graph_settings_menu;

static GfxWidgetSlot s_graph_slots[4];
static GfxWidgetSlot s_graph_settings_slots[3];

/* -- Graph tunables ---------------------------------------------- */

static GfxWidget *s_graph_widget;
static int        s_wpm_max        = 150;
static int        s_wpm_window_idx = 1;  /* default: 1 min */

static const int  WPM_WINDOW_SAMPLES[] = { 30, 60, 120, 300, 600 };
static const char *const WPM_WINDOW_LABELS[] = {
    "30s", "1 min", "2 min", "5 min", "10 min"
};
#define WPM_WINDOW_COUNT 5

static int  wpm_autoscale_get(void)
{
    return s_graph_widget ? ((GfxGraph *)s_graph_widget->data)->autoscale : 0;
}
static void wpm_autoscale_set_inner(int v)
{
    if (!s_graph_widget) return;
    ((GfxGraph *)s_graph_widget->data)->autoscale = v ? 1 : 0;
    GfxMarkDirty(s_graph_widget);
}

static int  wpm_max_get(void) { return s_wpm_max; }
static void wpm_max_set(int v)
{
    s_wpm_max = v;
    if (s_graph_widget) {
        ((GfxGraph *)s_graph_widget->data)->data_max = v;
        GfxMarkDirty(s_graph_widget);
    }
}

static int  wpm_legend_get(void)
{
    return s_graph_widget ? ((GfxGraph *)s_graph_widget->data)->show_legend : 0;
}
static void wpm_legend_set(int v)
{
    if (!s_graph_widget) return;
    ((GfxGraph *)s_graph_widget->data)->show_legend = v ? 1 : 0;
    GfxMarkDirty(s_graph_widget);
}

static int  wpm_window_get(void) { return s_wpm_window_idx; }
static void wpm_window_set(int v)
{
    s_wpm_window_idx = v;
    if (!s_graph_widget) return;
    int n = WPM_WINDOW_SAMPLES[v];
    GfxGraph *g = s_graph_widget->data;
    g->data       = &wpm_history[WPM_HISTORY_LEN - n];
    g->data_count = n;
    GfxMarkDirty(s_graph_widget);
}

static void wpm_autoscale_set_wrapped(int v);

static const GfxMenuItem GRAPH_SETTINGS_ITEMS[] = {
    { "Autoscale", GFX_MENU_TOGGLE,
      .toggle = { wpm_autoscale_get, wpm_autoscale_set_wrapped } },
    { "Max",       GFX_MENU_SLIDER,
      .slider = { wpm_max_get, wpm_max_set, 50, 300, 10 } },
    { "Legends",   GFX_MENU_TOGGLE,
      .toggle = { wpm_legend_get, wpm_legend_set } },
    { "Time",      GFX_MENU_CHOICE,
      .choice = { WPM_WINDOW_LABELS, WPM_WINDOW_COUNT,
                  wpm_window_get, wpm_window_set } },
};
#define GRAPH_SETTINGS_COUNT \
    ((int)(sizeof(GRAPH_SETTINGS_ITEMS)/sizeof(GRAPH_SETTINGS_ITEMS[0])))

/* Max row lives at items[] index 1 — hide it via the library's
 * hidden-mask when autoscale takes over. The library handles the
 * selection-clamping internally. */
#define GRAPH_SETTINGS_MAX_IDX 1

static void wpm_autoscale_set_wrapped(int v)
{
    wpm_autoscale_set_inner(v);
    if (s_graph_settings_menu)
        GfxMenuListSetHidden(s_graph_settings_menu, GRAPH_SETTINGS_MAX_IDX, v);
}

static void accent_graph(GfxWidget *w, GfxColor c)
{ ((GfxGraph *)w->data)->color = c;        GfxMarkDirty(w); }

static void bg_graph(GfxWidget *w, GfxColor c)
{ ((GfxGraph *)w->data)->bg_color = c;     GfxMarkDirty(w); }

static void secondary_graph(GfxWidget *w, GfxColor c)
{ ((GfxGraph *)w->data)->legend_color = c; GfxMarkDirty(w); }

void build_graph(void)
{
    GfxWidget *body = NewGfxGraph(
        .color = accent_color(), .bg_color = bg_color_value(),
        .line_width   = 2,
        .autoscale    = 1,
        .show_legend  = 1,
        .legend_color = secondary_color(),
        .legend_font  = &font_fira_mono_14,
        .data_min     = 0,
        .data_max     = 120,
        .data         = wpm_history,
        .data_count   = WPM_HISTORY_LEN,
    );
    settings_register_accent(body, accent_graph);
    settings_register_bg(body, bg_graph);
    settings_register_secondary(body, secondary_graph);
    s_graph_widget = body;
    wpm_window_set(s_wpm_window_idx); /* apply initial window to data/data_count */
    wpm_set_graph_widget(body);

    s_graph_slots[0] = (GfxWidgetSlot){ body,          BODY_SLOT };
    s_graph_slots[1] = (GfxWidgetSlot){ s_statusbar,   STATUSBAR_SLOT };
    s_graph_slots[2] = (GfxWidgetSlot){ s_navbar,      NAVBAR_SLOT };
    s_graph_slots[3] = (GfxWidgetSlot){ make_border(),  BORDER_SLOT };
    s_graph_scr = (GfxScreen){
        .name = "WPM Graph",
        .slots = s_graph_slots, .slot_count = 4,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_graph_scr);

    s_graph_settings_menu = make_menu(GRAPH_SETTINGS_ITEMS, GRAPH_SETTINGS_COUNT,
                                      GFX_MENU_INDICATOR_NONE);
    /* Initial visibility tracks the persisted autoscale setting so the
     * Max row is hidden at boot when autoscale is already on. */
    GfxMenuListSetHidden(s_graph_settings_menu, GRAPH_SETTINGS_MAX_IDX,
                         wpm_autoscale_get());
    settings_register_accent(s_graph_settings_menu, accent_menu);
    settings_register_bg(s_graph_settings_menu, bg_menu);
    s_graph_settings_slots[0] = (GfxWidgetSlot){ s_graph_settings_menu, MENU_SLOT };
    s_graph_settings_slots[1] = (GfxWidgetSlot){ s_navbar,              NAVBAR_SLOT };
    s_graph_settings_slots[2] = (GfxWidgetSlot){ make_border(),         BORDER_SLOT };
    s_graph_settings_scr = (GfxScreen){
        .name = "WPM Graph Settings",
        .slots = s_graph_settings_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_graph_settings_scr);
}
