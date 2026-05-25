#ifndef GFX_SCREEN_H
#define GFX_SCREEN_H

#include "gfx_widgets.h"

/* GfxScreen holds an array of these so we can deliver the correct tile
 * to each widget without the widget knowing its own position. */
typedef struct {
    GfxWidget      *widget;
    GfxBoundingBox  box;
} GfxWidgetSlot;

/* -- Screen ----------------------------------------------------------- */

typedef struct GfxScreen {
    const char        *name;
    GfxWidgetSlot     *slots;          /* array of (widget, box) pairs   */
    int                slot_count;
    u16                clear_color;    /* full-fb clear on screen change */
    struct GfxScreen  *parent;         /* set by GfxNavTo                */
} GfxScreen;

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
