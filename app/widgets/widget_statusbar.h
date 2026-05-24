/* Persistent bottom strip with caps-lock, connection, layer, and
 * battery indicators. Shared across every screen; no data source
 * yet — the four state fields are placeholders the renderer reads. */
#ifndef HELLO_WIDGET_STATUSBAR_H
#define HELLO_WIDGET_STATUSBAR_H

#include "gfx.h"

typedef enum {
    STATUSBAR_CONN_BLUETOOTH = 0,
    STATUSBAR_CONN_USB       = 1,
    STATUSBAR_CONN_WIRELESS  = 2,
} StatusBarConn;

typedef struct {
    const GfxFont *font;
    GfxColor       color;          /* active icons + text */
    GfxColor       color_dim;      /* inactive icons (caps off, etc.) */
    GfxColor       bg_color;
    bool           skip_clear;
    /* state — keyboard status via StatusBarBindCallbacks push callback */
    bool           caps_on;
    u8             conn;           /* StatusBarConn */
    u8             layer;
    /* WPM state — polled by StatusBarTick at ~10 Hz */
    u32            wpm_value;
    u32            wpm_check_ms;
} StatusBar;

void StatusBarDraw(GfxRenderingTile *tile, StatusBar *s);
int  StatusBarTick(StatusBar *s);

/* Hook up the SDK status / battery push callbacks so the widget updates
 * itself only on real state changes (no per-frame polling). Also syncs
 * the current SDK snapshot into the widget so we paint correct values
 * from the first frame rather than the construction defaults. Call
 * once after construction; only one statusbar instance is supported. */
void StatusBarBindCallbacks(GfxWidget *w);

#define NewStatusBar(...) \
    GfxNewWidget(sizeof(StatusBar), &(StatusBar){ __VA_ARGS__ }, \
                 GFX_DRAW_FN(StatusBarDraw), GFX_TICK_FN(StatusBarTick))

#endif
