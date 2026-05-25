#include "common.h"
#include "settings.h"
#include "wpm.h"
#include "widget_life.h"
#include "widget_statusbar.h"
#include "widget_battery.h"
#include "widget_navbar.h"
#include "text_log.h"
#include "screens/screen_bounce.h"
#include "screens/screen_wireframe.h"
#include "screens/screen_life.h"
#include "screens/screen_clock.h"
#include "screens/screen_graph.h"
#include "screens/screen_settings.h"
#include "screens/screen_fonts.h"
#include "screens/screen_text.h"
#include "screens/screen_main.h"

static GfxFb s_fb;

// s_overlay_bg is the background widget for Bounce and Wireframe
#define OVERLAY_BG_W  (LCD_W - 2 * BODY_INSET_X)
#define OVERLAY_BG_H  (EXPANDED_BODY_H)
static GfxFb           s_overlay_bg;
static u16             s_overlay_bg_pixels[OVERLAY_BG_W * OVERLAY_BG_H];
static GfxWidget      *s_overlay_bg_widget;
GfxRenderTarget        s_overlay_bg_target;


GfxWidget *s_navbar;
GfxWidget *s_statusbar;

/* -- Accent appliers -------------------------------------------- */

void accent_menu(GfxWidget *w, GfxColor c)
{ ((GfxMenuList *)w->data)->color_indicator = c; GfxMarkDirty(w); }

static void accent_statusbar(GfxWidget *w, GfxColor c)
{ ((StatusBar *)w->data)->color = c; GfxMarkDirty(w); }
static void accent_battery(GfxWidget *w, GfxColor c)
{ ((BatteryBadge *)w->data)->color = c; GfxMarkDirty(w); }

/* -- Bg appliers ------------------------------------------------ */

void bg_menu(GfxWidget *w, GfxColor c)
{ ((GfxMenuList *)w->data)->bg_color = c; GfxMarkDirty(w); }

void bg_clock(GfxWidget *w, GfxColor c)
{
    GfxClock *cl = w->data;
    cl->bg_color   = c;
    cl->cache_ready = 0;   /* force full repaint to overwrite stale pixels */
    GfxMarkDirty(w);
}

static void bg_breadcrumb(GfxWidget *w, GfxColor c)
{ ((GfxBreadcrumb *)w->data)->bg_color = c; GfxMarkDirty(w); }
static void bg_statusbar(GfxWidget *w, GfxColor c)
{ ((StatusBar *)w->data)->bg_color = c; GfxMarkDirty(w); }
static void bg_battery(GfxWidget *w, GfxColor c)
{ ((BatteryBadge *)w->data)->bg_color = c; GfxMarkDirty(w); }
static void bg_navbar(GfxWidget *w, GfxColor c)
{ ((NavBar *)w->data)->bg_color = c; GfxMarkDirty(w); }

/* -- Font appliers ---------------------------------------------- */

static void font_breadcrumb(GfxWidget *w, const GfxFont *f)
{ ((GfxBreadcrumb *)w->data)->font = f; GfxMarkDirty(w); }
static void font_battery(GfxWidget *w, const GfxFont *f)
{ ((BatteryBadge *)w->data)->font = f; GfxMarkDirty(w); }
static void font_menu(GfxWidget *w, const GfxFont *f)
{ ((GfxMenuList *)w->data)->font = f; GfxMarkDirty(w); }

/* -- Secondary appliers ---------------------------------------- */

void secondary_clock(GfxWidget *w, GfxColor c)
{
    GfxClock *cl = w->data;
    cl->color       = c;
    cl->cache_ready = 0;
    GfxMarkDirty(w);
}

/* -- Chrome factories ------------------------------------------- */

GfxWidget *make_border(void)
{
    GfxWidget *w = NewGfxBorder(
        .radius = GFX_CORNER_RADIUS, .thickness = 2,
        .color = frame_color_value(),
    );
    settings_register_border(w);
    return w;
}

static GfxWidget *make_breadcrumb(void)
{
    GfxWidget *w = NewGfxBreadcrumb(
        .font = &font_montserrat_14,
        .color = accent_color(), .bg_color = bg_color_value(),
        .fade_width = 24, .separator = " > ",
    );
    settings_register_breadcrumb(w);
    settings_register_bg(w, bg_breadcrumb);
    settings_register_header_font(w, font_breadcrumb);
    return w;
}

static void bg_textbox(GfxWidget *w, GfxColor c)
{ ((GfxTextbox *)w->data)->bg_color = c; GfxMarkDirty(w); }

GfxWidget *make_placeholder(const char *text)
{
    GfxWidget *w = NewGfxTextbox(
        .font   = &font_lora_24,
        .color  = GFX_GREY,
        .bg_color = bg_color_value(),
        .text   = text,
        .halign = GFX_ALIGN_CENTER,
        .valign = GFX_VALIGN_MIDDLE,
    );
    settings_register_bg(w, bg_textbox);
    return w;
}

/* Menu fills the body area down to ~y=170 — settings screens don't
 * carry the statusbar so we get the extra ~28 px to use. Tight item
 * spacing (4 px gutter, 24 px per row) lets the longer Settings list
 * fit 6 rows visible instead of 5 with the room we have. */
GfxWidget *make_menu(const GfxMenuItem *items, int n, GfxMenuIndicatorAlign align)
{
    /* Menu uses the full no-statusbar area, not BODY_TOP_Y — the
     * body shift that gives demos visual breathing space would just
     * eat rows the menu wants for items. */
    GfxWidget *w = NewGfxMenuList(
        .font = &font_montserrat_14,
        .color_normal             = GFX_GREY,
        .color_selected           = GFX_WHITE,
        .color_indicator          = accent_color(),
        .color_indicator_inactive = GFX_GREY,
        .bg_color                 = bg_color_value(),
        .items = items, .item_count = n,
        .item_spacing  = 4,
        .indicator_pad = 28,
        .indicator_align = align,
    );
    settings_register_list_font(w, font_menu);
    return w;
}

/* -- Input dispatch --------------------------------------------- */

#define HID_REPRESS_MS  80u

static int  s_asleep = 0;
static u32  s_last_activity_ms = 0;
static int  s_input_inverted = 1;
int  input_inverted_get(void)  { return s_input_inverted; }
void input_inverted_set(int v) { s_input_inverted = v ? 1 : 0; }

static GfxWidget *current_menu_widget(void)
{
    const GfxScreen *top = GfxCurrentScreen();
    if (top == &s_settings_scr)           return s_settings_menu;
    if (top == &s_debug_scr)              return s_debug_menu;
    if (top == &s_bounce_settings_scr)    return s_bounce_settings_menu;
    if (top == &s_life_settings_scr)      return s_life_settings_menu;
    if (top == &s_wireframe_settings_scr) return s_wireframe_settings_menu;
    if (top == &s_graph_settings_scr)     return s_graph_settings_menu;
    if (top == &s_fonts_scr)              return s_fonts_menu;
    return NULL;
}

static void handle_input(void)
{
    static u8  last_hid;
    static u32 last_hid_ms;

    kbd_event_t ev;
    while (kbd_event_pop(&ev)) {
        int meaningful = (ev.type == KBD_EV_LCD) ||
                         (ev.type == KBD_EV_KEY && ev.code != 0);
        if (meaningful) {
            s_last_activity_ms = ev.time_ms;
            if (s_asleep) {
                s_asleep = 0;
                lcd_wake();
                GfxForceFullRedraw();
                continue;
            }
        }

        const GfxScreen *top = GfxCurrentScreen();

        if (ev.type == KBD_EV_KEY) {
            if (ev.code == 0) continue;
            /* Debounce repeats so a held key doesn't spam WPM/life. */
            int fresh = (ev.code != last_hid
                      || (ev.time_ms - last_hid_ms) >= HID_REPRESS_MS);
            last_hid    = ev.code;
            last_hid_ms = ev.time_ms;
            if (!fresh) continue;
            wpm_record(ev.time_ms);

            /* Seed Life only while it's the active screen; typing on
             * other screens shouldn't quietly fill the grid. 1/N drops
             * a preset, otherwise a single-cell nudge. */
            if (s_life_widget && top == &s_life_scr) {
                LifeDemo *l = s_life_widget->data;
                if (life_spawn_get() > 0
                    && ((rand() & 0x7FFF) % life_spawn_get()) == 0) {
                    LifeDemoAddRandomPattern(l);
                } else {
                    int c = (rand() & 0x7FFF) % l->cols;
                    int r = (rand() & 0x7FFF) % l->rows;
                    l->grid[r * l->cols + c] = 1;
                }
                GfxMarkDirty(s_life_widget);
            }

            if (top == &s_text_scr) {
                text_log_hid(ev.code);
                GfxMarkDirty(s_text_widget);
            }
            continue;
        }
        if (ev.type != KBD_EV_LCD) continue;

        u8 lcd_code = ev.code;
        if (s_input_inverted) {
            if (lcd_code == KBD_LCD_UP)   lcd_code = KBD_LCD_DOWN;
            else if (lcd_code == KBD_LCD_DOWN) lcd_code = KBD_LCD_UP;
        }

        GfxWidget *menu = current_menu_widget();
        switch (lcd_code) {
        case KBD_LCD_UP:
            if (top == &s_main) {
                GfxCarouselPrev(s_main_carousel->data);
                GfxMarkDirty(s_main_carousel);
            } else if (menu) {
                GfxMenuListUp(menu->data);
                GfxMarkDirty(menu);
            }
            break;
        case KBD_LCD_DOWN:
            if (top == &s_main) {
                GfxCarouselNext(s_main_carousel->data);
                GfxMarkDirty(s_main_carousel);
            } else if (menu) {
                GfxMenuListDown(menu->data);
                GfxMarkDirty(menu);
            }
            break;
        case KBD_LCD_ENTER:
            if (top == &s_main) {
                GfxCarouselSelect(s_main_carousel->data);
            } else if (top == &s_bounce_scr) {
                GfxNavTo(&s_bounce_settings_scr);
            } else if (top == &s_life_scr) {
                GfxNavTo(&s_life_settings_scr);
            } else if (top == &s_wireframe_scr) {
                GfxNavTo(&s_wireframe_settings_scr);
            } else if (top == &s_graph_scr) {
                GfxNavTo(&s_graph_settings_scr);
            } else if (menu) {
                GfxMenuListEnter(menu);
            }
            break;
        case KBD_LCD_BACK:
            if (!GfxMenuListBack(menu)) GfxNavBack();
            break;
        default: break;
        }
    }
}

/* -- Sleep ------------------------------------------------------ */

void meletricks_sleep_now(void)
{
    s_asleep = 1;
    lcd_sleep();
}

static void sleep_tick(void)
{
    u32 now = fr_millis();
    if (s_last_activity_ms == 0) { s_last_activity_ms = now; return; }

    u32 timeout = sleep_timeout_ms();
    if (!s_asleep && timeout != 0 && (now - s_last_activity_ms) >= timeout) {
        s_asleep = 1;
        lcd_sleep();
    }
}

/* -- Overlay bg ------------------------------------------------- */

static void build_overlay_bg(void)
{
    GfxFbInit(&s_overlay_bg, s_overlay_bg_pixels, OVERLAY_BG_W, OVERLAY_BG_H);
    GfxFbClear(&s_overlay_bg, bg_color_value());

    s_overlay_bg_widget = NewGfxClock(
        .font = &font_montserrat_64,
        .color = secondary_color(),
        .bg_color = bg_color_value(),
        .show_seconds = clock_seconds_get(),
        .halign = GFX_ALIGN_CENTER,
        .valign = GFX_VALIGN_MIDDLE,
    );
    settings_register_bg(s_overlay_bg_widget, bg_clock);
    settings_register_secondary(s_overlay_bg_widget, secondary_clock);
    settings_register_clock(s_overlay_bg_widget);

    /* The target itself isn't registered globally — consumer widgets
     * pick it up via GfxAddWidgetDep, and the library walks visible
     * widgets' deps each tick. No consumer visible => bg doesn't tick. */
    s_overlay_bg_target = (GfxRenderTarget){
        .fb     = &s_overlay_bg,
        .source = s_overlay_bg_widget,
    };
}

static int s_fps_show;
int  fps_show_get(void)  { return s_fps_show; }
void fps_show_set(int v)
{
    int was = s_fps_show;
    s_fps_show = v ? 1 : 0;
    if (was && !s_fps_show) GfxForceFullRedraw();
}

static void fps_present_hook(GfxFb *fb)
{
    if (!s_fps_show) return;
    u32 v = GfxFps();
    if (v > 99u) v = 99u;
    char buf[3] = { '0' + (char)(v / 10u), '0' + (char)(v % 10u), 0 };
    int tw = GfxTextWidth(&font_fira_mono_14, buf);
    int x  = LCD_W - tw - 4;
    int y  = 2;
    GfxFillRect(fb, x - 2, y, tw + 4, font_fira_mono_14.line_height, GFX_BLACK);
    GfxDrawTextBg(fb, &font_fira_mono_14, x, y, buf, GFX_WHITE, GFX_BLACK);
}

static void build_overlays(void)
{
    /* Battery badge takes the top-right corner. Updates via SDK push
     * callback, not polling. */
    GfxWidget *s_battery_badge = NewBatteryBadge(
        .font = &font_montserrat_14,
        .color = accent_color(), .bg_color = bg_color_value(),
    );
    settings_register_accent(s_battery_badge, accent_battery);
    settings_register_bg(s_battery_badge, bg_battery);
    settings_register_header_font(s_battery_badge, font_battery);
    BatteryBadgeBindCallback(s_battery_badge);

    /* Statusbar sits 34 px tall at the bottom. Body shrinks (BODY_H
     * reduced) so the two regions touch cleanly at y = BODY_TOP_Y +
     * BODY_H = 136. Same instance referenced from every demo screen.
     * Status fields (caps / conn / layer) update via the SDK push
     * callback wired below — no per-frame polling. */
    s_statusbar = NewStatusBar(
        .font = &font_fira_mono_14,
        .color = accent_color(),
        .color_dim = GFX_GREY,
        .bg_color = bg_color_value(),
    );
    StatusBarBindCallbacks(s_statusbar);
    settings_register_accent(s_statusbar, accent_statusbar);
    settings_register_bg(s_statusbar, bg_statusbar);

    /* NavBar consolidates the upper bar — owns the breadcrumb and
     * the battery badge as children, and paints its own hairline
     * separator along its bottom. From the library's perspective
     * NavBar is one widget; child dispatch happens internally. */
    GfxWidget *s_breadcrumb = make_breadcrumb();
    s_navbar = NewNavBar(
        .bg_color = bg_color_value(),
        .separator_color = GFX_GREY,
    );
    NavBarAddChild(s_navbar, s_breadcrumb,
                   (GfxBoundingBox){ HEADER_BC_X, 6, HEADER_BC_W,
                                     font_montserrat_14.line_height });
    NavBarAddChild(s_navbar, s_battery_badge,
                   (GfxBoundingBox){ HEADER_BATT_X, 6, HEADER_BATT_W,
                                     font_fira_mono_14.line_height });
    settings_register_bg(s_navbar, bg_navbar);
}

/* -- Setup ------------------------------------------------------ */

/* Each screen builds its widget array as: body widgets first, then
 * the shared header overlays, breadcrumb, and border last so the
 * outline always wins (paint order = list order). */

FR_SETUP void hello_setup(void)
{
    gui_pause();
    lcd_te_sync_disable();
    srand(fr_micros());
    GfxFbInit(&s_fb, LCD_FB, LCD_W, LCD_H);

    build_overlay_bg();
    settings_bind_demo_clock_bg(&s_overlay_bg_target);
    build_overlays();
    build_bounce();
    build_wireframe();
    build_life();
    build_text();
    build_graph();
    build_clock();
    build_settings_screens();
    build_fonts_screen();
    build_main();
    GfxInit(&s_fb);
    GfxSetPresentHook(fps_present_hook);
    GfxNavTo(&s_main);
}

FR_TASK(hello_loop, 0x80000)
{
    handle_input();
    sleep_tick();
    if (!s_asleep) {
        wpm_history_tick();
        GfxTick();
    }
}
