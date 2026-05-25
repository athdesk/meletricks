#include "screen_wireframe.h"
#include "widget_wireframe.h"

GfxScreen  s_wireframe_scr;
GfxScreen  s_wireframe_settings_scr;
GfxWidget *s_wireframe_settings_menu;

static GfxWidgetSlot s_wireframe_slots[4];
static GfxWidgetSlot s_wireframe_settings_slots[3];

/* -- Wireframe tunables ------------------------------------------ */

static GfxWidget *s_wireframe_body;
static int        s_wireframe_idx;
static int        s_wireframe_size = 100;   /* focal % of min(w, h) */

static int  wireframe_shape_get(void) { return s_wireframe_idx; }
static void wireframe_shape_set(int v)
{
    if (v < 0 || v >= WIREFRAME_SHAPE_COUNT) return;
    s_wireframe_idx = v;
    if (s_wireframe_body) {
        WireframeDemoSetShape(s_wireframe_body->data, &WIREFRAME_SHAPES[v]);
        GfxMarkDirty(s_wireframe_body);
    }
}

static int  wireframe_size_get(void) { return s_wireframe_size; }
static void wireframe_size_set(int v)
{
    if (v < 10) v = 10;
    s_wireframe_size = v;
    if (s_wireframe_body) {
        ((WireframeDemo *)s_wireframe_body->data)->size_pct = v;
        GfxMarkDirty(s_wireframe_body);
    }
}

static void apply_bg_target(GfxWidget *w, const GfxRenderTarget *t)
{
    ((WireframeDemo *)w->data)->bg = t;
    GfxMarkDirty(w);
}

static const GfxMenuItem WIREFRAME_SETTINGS_ITEMS[] = {
    { "Shape", GFX_MENU_CHOICE,
      .choice = { WIREFRAME_SHAPE_LABELS, WIREFRAME_SHAPE_COUNT,
                  wireframe_shape_get,    wireframe_shape_set } },
    { "Size",  GFX_MENU_SLIDER,
      .slider = { wireframe_size_get, wireframe_size_set, 30, 200, 10 } },
    { "Clock bg", GFX_MENU_TOGGLE,
      .toggle = { demo_clock_bg_get, demo_clock_bg_set } },
};
#define WIREFRAME_SETTINGS_COUNT \
    ((int)(sizeof(WIREFRAME_SETTINGS_ITEMS)/sizeof(WIREFRAME_SETTINGS_ITEMS[0])))

static void accent_wireframe(GfxWidget *w, GfxColor c)
{ ((WireframeDemo *)w->data)->color = c;    GfxMarkDirty(w); }

static void bg_wireframe(GfxWidget *w, GfxColor c)
{ ((WireframeDemo *)w->data)->bg_color = c; GfxMarkDirty(w); }

void build_wireframe(void)
{
    GfxWidget *body = NewWireframeDemo(
        .color = accent_color(), .bg_color = bg_color_value(),
        .bg       = &s_overlay_bg_target,
        .shape    = &WIREFRAME_SHAPES[wireframe_shape_get()],
        .size_pct = wireframe_size_get(),
    );
    GfxAddWidgetDep(body, &s_overlay_bg_target);
    settings_register_accent(body, accent_wireframe);
    settings_register_bg(body, bg_wireframe);
    settings_register_bg_consumer(body, apply_bg_target);
    s_wireframe_body = body;

    s_wireframe_slots[0] = (GfxWidgetSlot){ body,         EXPANDED_BODY_SLOT };
    s_wireframe_slots[1] = (GfxWidgetSlot){ s_statusbar,  STATUSBAR_SLOT };
    s_wireframe_slots[2] = (GfxWidgetSlot){ s_navbar,     NAVBAR_SLOT };
    s_wireframe_slots[3] = (GfxWidgetSlot){ make_border(), BORDER_SLOT };
    s_wireframe_scr = (GfxScreen){
        .name = "Wireframe",
        .slots = s_wireframe_slots, .slot_count = 4,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_wireframe_scr);

    s_wireframe_settings_menu = make_menu(WIREFRAME_SETTINGS_ITEMS,
                                          WIREFRAME_SETTINGS_COUNT,
                                          GFX_MENU_INDICATOR_NONE);
    settings_register_accent(s_wireframe_settings_menu, GFX_APPLIER_FN(GfxMenuList, ColorIndicator));
    settings_register_bg(s_wireframe_settings_menu, GFX_APPLIER_FN(GfxMenuList, BgColor));
    s_wireframe_settings_slots[0] = (GfxWidgetSlot){ s_wireframe_settings_menu, MENU_SLOT };
    s_wireframe_settings_slots[1] = (GfxWidgetSlot){ s_navbar,                  NAVBAR_SLOT };
    s_wireframe_settings_slots[2] = (GfxWidgetSlot){ make_border(),             BORDER_SLOT };
    s_wireframe_settings_scr = (GfxScreen){
        .name = "Wireframe Settings",
        .slots = s_wireframe_settings_slots, .slot_count = 3,
        .clear_color = bg_color_value(),
    };
    settings_register_screen(&s_wireframe_settings_scr);
}
