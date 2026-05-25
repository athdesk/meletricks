#ifndef GFX_WIDGETS_H
#define GFX_WIDGETS_H

#include "gfx_fb.h"
#include "gfx_font.h"
#include "gfx_shapes.h"

typedef enum {
    GFX_ALIGN_LEFT   = 0,
    GFX_ALIGN_CENTER = 1,
    GFX_ALIGN_RIGHT  = 2,
} GfxHalign;

typedef enum {
    GFX_VALIGN_TOP    = 0,
    GFX_VALIGN_MIDDLE = 1,
    GFX_VALIGN_BOTTOM = 2,
} GfxValign;

/* Cast helpers to avoid warnings */
#define GFX_DRAW_FN(fn) ((GfxWidgetDrawFn)(void *)(fn))
#define GFX_TICK_FN(fn) ((GfxWidgetTickFn)(void *)(fn))

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

/* Heap-allocate { GfxWidget, data block } in one call. `data_init` is
 * copied into the new data buffer.
 * This is needed to provide a standard way to allocate widgets with
 * differently sized data types. */
GfxWidget *GfxNewWidget(u32 data_size, const void *data_init,
                        GfxWidgetDrawFn draw, GfxWidgetTickFn tick);

void GfxMarkDirty   (GfxWidget *w);
void GfxDeleteWidget(GfxWidget *w);

/* Register `t` as a render target this widget reads from. Silently
 * drops if dep_count is already at GFX_WIDGET_MAX_DEPS. */
void GfxAddWidgetDep(GfxWidget *w, GfxRenderTarget *t);

#define GFX_APPLIER_FN(WidgetType, Field) \
    _GfxGeneratedApplier_##WidgetType##_##Field

#define GFX_DEFINE_APPLIER(WidgetType, Field, ...)                        \
    static void __attribute__((unused))                                    \
    GFX_APPLIER_FN(WidgetType, Field)(GfxWidget *w,                      \
        __typeof__(((WidgetType *)0)->Field) _val)                        \
    {                                                                      \
        WidgetType *_d = (WidgetType *)w->data;                           \
        _d->Field = _val;                                                  \
        __VA_ARGS__                                                        \
        GfxMarkDirty(w);                                                   \
    }

#include "widgets/border.h"
#include "widgets/textbox.h"
#include "widgets/menu_list.h"
#include "widgets/clock.h"
#include "widgets/graph.h"
#include "widgets/progress.h"
#include "widgets/marquee.h"
#include "widgets/breadcrumb.h"
#include "gfx_icon.h"
#include "widgets/carousel.h"

#endif
