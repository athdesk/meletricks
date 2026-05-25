#ifndef GFX_SCREEN_H
#define GFX_SCREEN_H

#include "gfx_fb.h"

#ifndef true
#define true  1
#define false 0
#endif

struct GfxWidget;

/* Full rendering context delivered to every widget draw function */
typedef struct {
    struct GfxFb  *fb;
    GfxBoundingBox box;
} GfxRenderingTile;

/* GfxScreen holds an array of these so we can deliver the correct tile
 * to each widget without the widget knowing its own position. */
typedef struct {
    struct GfxWidget *widget;
    GfxBoundingBox    box;
} GfxWidgetSlot;

/* -- Allocators -------------------------------------------------------
 *
 * Defaults to ke_malloc on pool 0. We may want to change this to our own allocator */
#ifndef GfxAlloc
void *ke_malloc(u32 size, u8 type);
void  ke_free(void *ptr);
#define GfxAlloc(sz)  ke_malloc((u32)(sz), 0)
#define GfxFree(p)    ke_free(p)
#endif

/* Cast helpers to avoid warnings */
#define GFX_DRAW_FN(fn) ((GfxWidgetDrawFn)(void *)(fn))
#define GFX_TICK_FN(fn) ((GfxWidgetTickFn)(void *)(fn))

/* -- Widget contract -------------------------------------------------- */

typedef void (*GfxWidgetDrawFn)(GfxRenderingTile *tile, void *data);
typedef int  (*GfxWidgetTickFn)(void *data);   /* return 1 = mark dirty */

typedef struct GfxRenderTarget GfxRenderTarget;

/* Inline cap on per-widget dependency count. Most widgets waste this space.
 * Let's consider a dynamic allocation if this grows too large. */
#define GFX_WIDGET_MAX_DEPS 2

typedef struct GfxWidget {
    GfxWidgetDrawFn   draw;
    GfxWidgetTickFn   tick;        /* NULL = static, no auto-dirty      */
    void             *data;
    bool              dirty;       /* GfxTick reads + clears            */
    /* The library ensures each render target is drawn exactly once per frame
     * before any widget dependant on it. */
    u8                dep_count;
    GfxRenderTarget  *deps[GFX_WIDGET_MAX_DEPS];
} GfxWidget;

/* Heap-allocate { GfxWidget, data block } in one call. `data_init` is
 * copied into the new data buffer.
 * This is needed to provide a standard way to allocate widgets with
 * differently sized data types. */
GfxWidget *GfxNewWidget(u32 data_size, const void *data_init,
                        GfxWidgetDrawFn draw, GfxWidgetTickFn tick);

void GfxMarkDirty   (GfxWidget *w);
void DeleteGfxWidget(GfxWidget *w);

/* Register `t` as a render target this widget reads from. Silently
 * drops if dep_count is already at GFX_WIDGET_MAX_DEPS. */
void GfxAddWidgetDep(GfxWidget *w, GfxRenderTarget *t);

/* -- Screen ----------------------------------------------------------- */

typedef struct GfxScreen {
    const char        *name;
    GfxWidgetSlot     *slots;          /* array of (widget, box) pairs   */
    int                slot_count;
    u16                clear_color;    /* full-fb clear on screen change */
    struct GfxScreen  *parent;         /* set by GfxNavTo                */
} GfxScreen;

/* -- Render targets --------------------------------------------------- *
 * Not registered globally — consumer widgets list it in their deps.
 * Sources are supposed to fill the entire off-screen fb, so its tile
 * gets synthesized to be 1:1 with the framebuffer when calling source->draw.
 * An off-screen GfxFb paired with a source widget. */
struct GfxRenderTarget {
    struct GfxFb *fb;
    GfxWidget    *source;
    u32           version;
    /* internal — touched only by GfxTick. */
    u32           last_ticked_frame;
};

/* -- Runtime ---------------------------------------------------------- */

/* Hook the library to the output framebuffer. Call once before GfxTick. */
void GfxInit(struct GfxFb *fb);

/* App should call this once per main-loop iteration. */
void GfxTick(void);

void GfxForceFullRedraw(void);

void GfxNavTo  (GfxScreen *target);
void GfxNavBack(void);
GfxScreen *GfxCurrentScreen(void);

/* Frames-per-second over a 1-second sliding window. */
u32 GfxFps(void);

/* Hook called immediately before m_draw_frame on every presented tick. */
typedef void (*GfxPresentHookFn)(struct GfxFb *fb);
void GfxSetPresentHook(GfxPresentHookFn fn);

void GfxSetDebugFill    (int enabled);
int  GfxGetDebugFill    (void);
void GfxSetTickGateIdle (int enabled);
int  GfxGetTickGateIdle (void);
void GfxSetAlwaysPresent(int enabled);
int  GfxGetAlwaysPresent(void);

int GfxIsScreenChangeFrame(void);

#endif
