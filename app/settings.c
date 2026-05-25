#include "settings.h"
#include "gfx_screen.h"   /* GfxForceFullRedraw */

/* -- Foreground accent chooser ----------------------------------- *
 * Solarized-style palette — the accent rides on top of (potentially
 * coloured) backgrounds, so saturated solarized hues read better
 * than the raw RGB565 primaries. RGB565 values precomputed from
 * solarized 24-bit hex via GfxRgb. */

const char *const FG_COLOR_LABELS[] = {
    "Cyan", "Blue", "Violet", "Magenta",
    "Red",  "Orange", "Yellow", "Green",
};
static const GfxColor FG_COLOR_VALUES[] = {
    0x2D13,    /* Cyan    — solarized #2aa198 */
    0x245A,    /* Blue    — solarized #268bd2 */
    0x6B98,    /* Violet  — solarized #6c71c4 */
    0xD1B0,    /* Magenta — solarized #d33682 */
    0xD985,    /* Red     — solarized #dc322f */
    0xCA42,    /* Orange  — solarized #cb4b16 */
    0xB440,    /* Yellow  — solarized #b58900 */
    0x84C0,    /* Green   — solarized #859900 */
};

/* Default to Orange — pops against the default Slate background
 * without being as eye-watering as the bright primary colours. */
static int s_fg_idx = 5;

GfxColor accent_color(void)        { return FG_COLOR_VALUES[s_fg_idx]; }
int      fg_color_get(void)        { return s_fg_idx; }

#define MAX_ACCENT      16
#define MAX_BREADCRUMBS 16
#define MAX_BORDERS     16

typedef struct { GfxWidget *w; ColorApplyFn fn; } ColorEntry;

static ColorEntry s_accent[MAX_ACCENT];
static int         s_accent_n;
static GfxWidget  *s_breadcrumbs[MAX_BREADCRUMBS];
static int         s_breadcrumbs_n;
static GfxWidget  *s_borders[MAX_BORDERS];
static int         s_borders_n;

void settings_register_accent(GfxWidget *w, ColorApplyFn fn)
{
    if (s_accent_n < MAX_ACCENT) s_accent[s_accent_n++] = (ColorEntry){ w, fn };
}
void settings_register_breadcrumb(GfxWidget *w)
{
    if (s_breadcrumbs_n < MAX_BREADCRUMBS) s_breadcrumbs[s_breadcrumbs_n++] = w;
}
void settings_register_border(GfxWidget *w)
{
    if (s_borders_n < MAX_BORDERS) s_borders[s_borders_n++] = w;
}

void fg_color_set(int v)
{
    if (v < 0 || v >= FG_COLOR_COUNT) return;
    s_fg_idx = v;
    GfxColor c = FG_COLOR_VALUES[v];
    for (int i = 0; i < s_accent_n; i++) {
        s_accent[i].fn(s_accent[i].w, c);
    }
    for (int i = 0; i < s_breadcrumbs_n; i++) {
        ((GfxBreadcrumb *)s_breadcrumbs[i]->data)->Color = c;
        GfxMarkDirty(s_breadcrumbs[i]);
    }
}

/* -- Secondary accent chooser ------------------------------------ *
 * Distinct palette from FG — leads with White + grey since
 * value-side text reads best as neutral on most backgrounds, then
 * the same solarized accents for users who want a colour. */

const char *const SECONDARY_COLOR_LABELS[] = {
    "White", "Grey",
    "Cyan", "Blue", "Violet", "Magenta",
    "Red", "Orange", "Yellow", "Green",
};
static const GfxColor SECONDARY_COLOR_VALUES[] = {
    GFX_WHITE,
    GFX_GREY,  /* Grey   — GfxRgb(128, 128, 128)                  */
    0x2D13,    /* Cyan   — solarized #2aa198                      */
    0x245A,    /* Blue   — solarized #268bd2                      */
    0x6B98,    /* Violet — solarized #6c71c4                      */
    0xD1B0,    /* Magenta — solarized #d33682                     */
    0xD985,    /* Red    — solarized #dc322f                      */
    0xCA42,    /* Orange — solarized #cb4b16                      */
    0xB440,    /* Yellow — solarized #b58900                      */
    0x84C0,    /* Green  — solarized #859900                      */
};

static int s_secondary_idx = 1; /* default: Grey */

GfxColor secondary_color(void)     { return SECONDARY_COLOR_VALUES[s_secondary_idx]; }
int      secondary_color_get(void) { return s_secondary_idx; }

#define MAX_SECONDARY 16
static ColorEntry s_secondary[MAX_SECONDARY];
static int            s_secondary_n;

void settings_register_secondary(GfxWidget *w, ColorApplyFn fn)
{
    if (s_secondary_n < MAX_SECONDARY) {
        s_secondary[s_secondary_n++] = (ColorEntry){ w, fn };
    }
}

void secondary_color_set(int v)
{
    if (v < 0 || v >= SECONDARY_COLOR_COUNT) return;
    s_secondary_idx = v;
    GfxColor c = SECONDARY_COLOR_VALUES[v];
    for (int i = 0; i < s_secondary_n; i++) {
        s_secondary[i].fn(s_secondary[i].w, c);
    }
}

/* -- Background colour chooser ----------------------------------- */

const char *const BG_COLOR_LABELS[] = {
    "Black", "Soft Gray", "Slate", "Navy", "Forest", "Plum",
};
static const GfxColor BG_COLOR_VALUES[] = {
    GFX_BLACK,
    0x4208,    /* Soft Gray — GfxRgb( 64,  64,  64)      ~25% grey  */
    0x01A8,    /* Slate     — solarized base02 #073642             */
    0x0014,    /* Navy      — GfxRgb(  0,   0, 160)                */
    0x0280,    /* Forest    — GfxRgb(  0,  80,   0)                */
    0x2805,    /* Plum      — GfxRgb( 40,   0,  40)                */
};

/* Default to Slate — solarized's signature dark background, reads
 * well against the warm accent colours. */
static int s_bg_idx = 2;

GfxColor bg_color_value(void) { return BG_COLOR_VALUES[s_bg_idx]; }
int      bg_color_get(void)   { return s_bg_idx; }

#define MAX_BG_APPLY  32
#define MAX_SCREENS   16

static ColorEntry s_bg_apply[MAX_BG_APPLY];
static int        s_bg_apply_n;
static GfxScreen *s_screens[MAX_SCREENS];
static int        s_screens_n;

void settings_register_bg(GfxWidget *w, ColorApplyFn fn)
{
    if (s_bg_apply_n < MAX_BG_APPLY) s_bg_apply[s_bg_apply_n++] = (ColorEntry){ w, fn };
}
void settings_register_screen(GfxScreen *s)
{
    if (s_screens_n < MAX_SCREENS) s_screens[s_screens_n++] = s;
}

void bg_color_set(int v)
{
    if (v < 0 || v >= BG_COLOR_COUNT) return;
    s_bg_idx = v;
    GfxColor c = BG_COLOR_VALUES[v];

    for (int i = 0; i < s_bg_apply_n; i++) {
        s_bg_apply[i].fn(s_bg_apply[i].w, c);
    }
    for (int i = 0; i < s_screens_n; i++) {
        s_screens[i]->clear_color = c;
    }
    GfxColor frame_c = frame_color_value();
    for (int i = 0; i < s_borders_n; i++) {
        ((GfxBorder *)s_borders[i]->data)->Color = frame_c;
        GfxMarkDirty(s_borders[i]);
    }
    GfxForceFullRedraw();
}

/* -- Frame (border) colour chooser ------------------------------- */

const char *const FRAME_COLOR_LABELS[] = {
    "None", "White", "Gray", "Cyan", "Blue", "Yellow",
};
static const GfxColor FRAME_COLOR_VALUES[] = {
    0x0000,    /* None — resolved to bg_color_value() at read time   */
    GFX_WHITE,
    GFX_GREY,
    0x2D13,    /* Solarized Cyan   */
    0x245A,    /* Solarized Blue   */
    0xB440,    /* Solarized Yellow */
};

static int s_frame_idx;

int      frame_color_get(void) { return s_frame_idx; }
GfxColor frame_color_value(void)
{
    return (s_frame_idx == 0) ? bg_color_value() : FRAME_COLOR_VALUES[s_frame_idx];
}
void frame_color_set(int v)
{
    if (v < 0 || v >= FRAME_COLOR_COUNT) return;
    s_frame_idx = v;
    GfxColor c = frame_color_value();
    for (int i = 0; i < s_borders_n; i++) {
        ((GfxBorder *)s_borders[i]->data)->Color = c;
        GfxMarkDirty(s_borders[i]);
    }
}

/* -- Demo clock background toggle -------------------------------- *
 * Cross-screen: both Bounce and Wireframe show the overlay-bg clock.
 * Each screen registers its body widget via settings_register_bg_consumer;
 * demo_clock_bg_set walks the list to push the target (or NULL) to all. */

static int                    s_demo_clock_bg_on = 1;
static const GfxRenderTarget *s_demo_clock_bg;

#define MAX_BG_CONSUMERS 4
typedef struct { GfxWidget *w; BgTargetApplyFn fn; } BgConsumerEntry;
static BgConsumerEntry s_bg_consumers[MAX_BG_CONSUMERS];
static int             s_bg_consumers_n;

void settings_register_bg_consumer(GfxWidget *w, BgTargetApplyFn fn)
{
    if (s_bg_consumers_n < MAX_BG_CONSUMERS)
        s_bg_consumers[s_bg_consumers_n++] = (BgConsumerEntry){ w, fn };
}

int  demo_clock_bg_get(void) { return s_demo_clock_bg_on; }
void demo_clock_bg_set(int v)
{
    s_demo_clock_bg_on = v ? 1 : 0;
    const GfxRenderTarget *t = v ? s_demo_clock_bg : NULL;
    for (int i = 0; i < s_bg_consumers_n; i++)
        s_bg_consumers[i].fn(s_bg_consumers[i].w, t);
}

void settings_bind_demo_clock_bg(const GfxRenderTarget *t) { s_demo_clock_bg = t; }

/* -- Clock seconds toggle ---------------------------------------- */

#define MAX_CLOCKS 4
static GfxWidget *s_clocks[MAX_CLOCKS];
static int        s_clocks_n;
static int        s_clock_seconds = 0;   /* default off */

int  clock_seconds_get(void) { return s_clock_seconds; }
void clock_seconds_set(int v)
{
    s_clock_seconds = v ? 1 : 0;
    for (int i = 0; i < s_clocks_n; i++) {
        GfxClock *cl = s_clocks[i]->data;
        cl->show_seconds = (u8)s_clock_seconds;
        cl->cache_ready = 0;
        GfxMarkDirty(s_clocks[i]);
    }
}

void settings_register_clock(GfxWidget *w)
{
    if (!w) return;
    if (s_clocks_n < MAX_CLOCKS) s_clocks[s_clocks_n++] = w;
}

/* -- Font registries -------------------------------------------- */

#define MAX_HEADER_FONTS   4
#define MAX_LIST_FONTS     24
#define MAX_CAROUSEL_FONTS 2

typedef struct { GfxWidget *w; FontApplyFn fn; } FontEntry;

static FontEntry s_header_fonts[MAX_HEADER_FONTS];
static int       s_header_fonts_n;
static FontEntry s_list_fonts[MAX_LIST_FONTS];
static int       s_list_fonts_n;
static FontEntry s_carousel_fonts[MAX_CAROUSEL_FONTS];
static int       s_carousel_fonts_n;

void settings_register_header_font(GfxWidget *w, FontApplyFn fn)
{
    if (s_header_fonts_n < MAX_HEADER_FONTS)
        s_header_fonts[s_header_fonts_n++] = (FontEntry){ w, fn };
}
void settings_register_list_font(GfxWidget *w, FontApplyFn fn)
{
    if (s_list_fonts_n < MAX_LIST_FONTS)
        s_list_fonts[s_list_fonts_n++] = (FontEntry){ w, fn };
}
void settings_register_carousel_font(GfxWidget *w, FontApplyFn fn)
{
    if (s_carousel_fonts_n < MAX_CAROUSEL_FONTS)
        s_carousel_fonts[s_carousel_fonts_n++] = (FontEntry){ w, fn };
}

void settings_apply_header_font(const GfxFont *f)
{
    for (int i = 0; i < s_header_fonts_n; i++)
        s_header_fonts[i].fn(s_header_fonts[i].w, f);
}
void settings_apply_list_font(const GfxFont *f)
{
    for (int i = 0; i < s_list_fonts_n; i++)
        s_list_fonts[i].fn(s_list_fonts[i].w, f);
}
void settings_apply_carousel_font(const GfxFont *f)
{
    for (int i = 0; i < s_carousel_fonts_n; i++)
        s_carousel_fonts[i].fn(s_carousel_fonts[i].w, f);
}

/* -- Sleep timeout chooser --------------------------------------- */

const char *const SLEEP_TIMEOUT_LABELS[] = {
    "Off", "1 min", "2 min", "5 min", "10 min",
};
static const u32 SLEEP_TIMEOUT_MS_VALUES[] = {
    0, 60000, 120000, 300000, 600000,
};
static int s_sleep_timeout_idx = 3;   /* default: 5 min */

int  sleep_timeout_idx_get(void) { return s_sleep_timeout_idx; }
void sleep_timeout_idx_set(int v)
{
    if (v < 0 || v >= SLEEP_TIMEOUT_COUNT) return;
    s_sleep_timeout_idx = v;
}
u32  sleep_timeout_ms(void) { return SLEEP_TIMEOUT_MS_VALUES[s_sleep_timeout_idx]; }
