/* gfx_screen — widget/screen runtime + nav linking.
 *
 * Each tick the library walks the active screen's slot list, calls
 * each widget's tick function (which may set dirty flag), then renders
 * dirty widgets in slot order. Presents only when something was painted.
 */

#include "gfx.h"
#include "gfx_screen.h"
#include "zoomtkldyna.h"     /* m_draw_frame, lcd_is_idle */
#include "timer.h"           /* fr_millis */
#include <string.h>

static GfxFb     *g_fb;
static GfxScreen *g_current;
static GfxScreen *g_last_drawn;

static u32 s_fps_frames;
static u32 s_fps_window_start;
static u32 s_fps_value;

u32 GfxFps(void) { return s_fps_value; }

static u8 s_debug_fill;
void GfxSetDebugFill(int v) { s_debug_fill = v ? 1 : 0; }
int  GfxGetDebugFill(void)  { return s_debug_fill; }

static u8 s_tick_gate_idle = 1;
void GfxSetTickGateIdle(int v) { s_tick_gate_idle = v ? 1 : 0; }
int  GfxGetTickGateIdle(void)  { return s_tick_gate_idle; }

static u8 s_screen_change_frame;
int GfxIsScreenChangeFrame(void) { return s_screen_change_frame; }

static GfxPresentHookFn s_present_hook;
void GfxSetPresentHook(GfxPresentHookFn fn) { s_present_hook = fn; }

static u8 s_always_present;
void GfxSetAlwaysPresent(int v) { s_always_present = v ? 1 : 0; }
int  GfxGetAlwaysPresent(void)  { return s_always_present; }

/* This will wrap around in like 3 years if drawn at 60 fps */
static u32 s_frame;

void GfxAddWidgetDep(GfxWidget *w, GfxRenderTarget *t)
{
    if (!w || !t) return;
    if (w->dep_count >= GFX_WIDGET_MAX_DEPS) return;
    w->deps[w->dep_count++] = t;
}

/* Tick a target's source (and dependencies, recursively).
 * Redraws into target->fb when dirty and bumps version.
 * Setting last_ticked_frame *before* recursing
 * also serves as the cycle break. */
static void ensure_ticked(GfxRenderTarget *t)
{
    if (!t || !t->source || !t->fb) return;
    if (t->last_ticked_frame == s_frame) return;
    t->last_ticked_frame = s_frame;

    GfxWidget *w = t->source;
    for (int i = 0; i < w->dep_count; i++) ensure_ticked(w->deps[i]);

    if (w->tick && w->tick(w->data)) w->dirty = true;
    if (!w->dirty) return;
    GfxRenderingTile tile = { t->fb,
        { 0, 0, (int)t->fb->width, (int)t->fb->height } };
    if (w->draw) w->draw(&tile, w->data);
    w->dirty = 0;
    t->version++;
}

static void tick_visible_deps(const GfxScreen *s)
{
    for (int i = 0; i < s->slot_count; i++) {
        GfxWidget *w = s->slots[i].widget;
        if (!w) continue;
        for (int j = 0; j < w->dep_count; j++) ensure_ticked(w->deps[j]);
    }
}

void GfxInit(GfxFb *fb)
{
    g_fb         = fb;
    g_current    = NULL;
    g_last_drawn = NULL;
}

GfxWidget *GfxNewWidget(u32 data_size, const void *data_init,
                        GfxWidgetDrawFn draw, GfxWidgetTickFn tick)
{
    GfxWidget *w = GfxAlloc(sizeof(GfxWidget));
    if (!w) return NULL;
    void *d = NULL;
    if (data_size) d = GfxAlloc(data_size);
    if (!d && data_size) { GfxFree(w); return NULL; }
    if (data_size) memcpy(d, data_init, data_size);
    w->draw      = draw;
    w->tick      = tick;
    w->data      = d;
    w->dirty     = 1;
    w->dep_count = 0;
    return w;
}

void DeleteGfxWidget(GfxWidget *w)
{
    if (!w) return;
    if (w->data) GfxFree(w->data);
    GfxFree(w);
}

void GfxMarkDirty(GfxWidget *w) { if (w) w->dirty = true; }

void GfxForceFullRedraw(void) { g_last_drawn = NULL; }

GfxScreen *GfxCurrentScreen(void) { return g_current; }

void GfxNavTo(GfxScreen *target)
{
    if (!target || target == g_current) return;

    /* If target is already an ancestor, this is "go back up to it" —
     * cut the chain at target rather than creating a cycle. */
    for (GfxScreen *s = g_current; s; s = s->parent) {
        if (s == target) { g_current = target; return; }
    }

    target->parent = g_current;
    g_current      = target;
}

void GfxNavBack(void)
{
    if (g_current && g_current->parent) g_current = g_current->parent;
}

static void update_fps_counter(void)
{
    s_fps_frames++;
    u32 now = fr_millis();
    if (s_fps_window_start == 0) s_fps_window_start = now;
    u32 span = now - s_fps_window_start;
    if (span >= 1000u) {
        s_fps_value = (s_fps_frames * 1000u) / span;
        s_fps_frames = 0;
        s_fps_window_start = now;
    }
}

static void do_present(void)
{
    if (s_present_hook) s_present_hook(g_fb);
    m_draw_frame((const u8 *)g_fb->pixels);
    update_fps_counter();
}

void GfxTick(void)
{
    if (!g_fb || !g_current) return;
    GfxScreen *s = g_current;
    int screen_changed = (s != g_last_drawn);

    s_frame++;
    tick_visible_deps(s);
    if (s_tick_gate_idle && !lcd_is_idle()) return;

    if (s_debug_fill) {
        /* The purpose of this is to understand what is being actually written to framebuffer
         * at every refresh. In order to be efficient with our memory bandwidth. */
        GfxFbClear(g_fb, GFX_MAGENTA);
    }

    bool any_dirty = 0;

    if (screen_changed) {
        GfxFbClear(g_fb, s->clear_color);
        s_screen_change_frame = 1;
    }

    for (int i = 0; i < s->slot_count; i++) {
        GfxWidget *w = s->slots[i].widget;
        if (!w) continue;
        if ((w->tick && w->tick(w->data) || screen_changed) || w->dirty) {
            w->dirty = true;
            any_dirty = true;
        }
    }

    if (!any_dirty && !s_always_present) return;

    for (int i = 0; i < s->slot_count; i++) {
        GfxWidgetSlot *sl = &s->slots[i];
        if (!sl->widget || !sl->widget->dirty || !sl->widget->draw) continue;
        GfxRenderingTile tile = { g_fb, sl->box };
        sl->widget->draw(&tile, sl->widget->data);
        sl->widget->dirty = 0;
    }
    do_present();
    g_last_drawn = s;
    s_screen_change_frame = 0;
}
