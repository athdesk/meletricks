/* Settings state: global palette, sleep, clock seconds. Screen-specific
 * tunables (WPM graph, Life, Wireframe) live in their own screen .c files. */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "gfx.h"

GfxColor accent_color(void);
GfxColor bg_color_value(void);
GfxColor secondary_color(void);

extern const char *const FG_COLOR_LABELS[];
#define FG_COLOR_COUNT 8
int  fg_color_get(void);
void fg_color_set(int v);

extern const char *const SECONDARY_COLOR_LABELS[];
#define SECONDARY_COLOR_COUNT 10
int  secondary_color_get(void);
void secondary_color_set(int v);

extern const char *const BG_COLOR_LABELS[];
#define BG_COLOR_COUNT 6
int  bg_color_get(void);
void bg_color_set(int v);

extern const char *const FRAME_COLOR_LABELS[];
#define FRAME_COLOR_COUNT 6
GfxColor frame_color_value(void);
int      frame_color_get(void);
void     frame_color_set(int v);

int  demo_clock_bg_get(void);
void demo_clock_bg_set(int v);

int  clock_seconds_get(void);
void clock_seconds_set(int v);

extern const char *const SLEEP_TIMEOUT_LABELS[];
#define SLEEP_TIMEOUT_COUNT 5
int  sleep_timeout_idx_get(void);
void sleep_timeout_idx_set(int v);
u32  sleep_timeout_ms(void);    /* 0 = disabled */

void settings_register_clock(GfxWidget *w);

void settings_bind_demo_clock_bg(const GfxRenderTarget *t);

/* Registry for widgets whose bg-target field tracks demo_clock_bg_set.
 * Bounce and Wireframe each register their body widget here instead of
 * going through the old settings_bind_anim / settings_bind_wireframe. */
typedef void (*BgTargetApplyFn)(GfxWidget *w, const GfxRenderTarget *t);
void settings_register_bg_consumer(GfxWidget *w, BgTargetApplyFn fn);

typedef void (*AccentApplyFn)(GfxWidget *w, GfxColor c);
void settings_register_accent    (GfxWidget *w, AccentApplyFn fn);
void settings_register_breadcrumb(GfxWidget *w);
void settings_register_border    (GfxWidget *w);

typedef void (*SecondaryApplyFn)(GfxWidget *w, GfxColor c);
void settings_register_secondary(GfxWidget *w, SecondaryApplyFn fn);

typedef void (*BgApplyFn)(GfxWidget *w, GfxColor c);
void settings_register_bg    (GfxWidget *w, BgApplyFn fn);
void settings_register_screen(GfxScreen *s);

typedef void (*FontApplyFn)(GfxWidget *w, const GfxFont *f);
void settings_register_header_font  (GfxWidget *w, FontApplyFn fn);
void settings_register_list_font    (GfxWidget *w, FontApplyFn fn);
void settings_register_carousel_font(GfxWidget *w, FontApplyFn fn);
void settings_apply_header_font  (const GfxFont *f);
void settings_apply_list_font    (const GfxFont *f);
void settings_apply_carousel_font(const GfxFont *f);

#endif
