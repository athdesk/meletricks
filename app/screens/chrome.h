#ifndef SCREENS_CHROME_H
#define SCREENS_CHROME_H

#include "gfx.h"
#include "common.h"
#include "settings.h"

/* -- Screen slot bounding boxes ---------------------------------------
 * Expand to a brace-initializer for GfxBoundingBox { x, y, w, h }.
 * Use as the second element of a GfxWidgetSlot compound literal:
 *   (GfxWidgetSlot){ widget_ptr, BODY_SLOT }                          */
#define BORDER_SLOT        { 0, 0, LCD_W, LCD_H }
#define NAVBAR_SLOT        { 0, 0, LCD_W, EXPANDED_BODY_TOP_Y }
#define STATUSBAR_SLOT     { 12, LCD_H-36, LCD_W-24, 34 }
#define BODY_SLOT          { BODY_INSET_X, BODY_TOP_Y, LCD_W-2*BODY_INSET_X, BODY_H }
#define EXPANDED_BODY_SLOT { BODY_INSET_X, EXPANDED_BODY_TOP_Y, LCD_W-2*BODY_INSET_X, EXPANDED_BODY_H }
#define MENU_SLOT          { 12, 32, LCD_W-24, 138 }
#define CAROUSEL_SLOT      { 0, BODY_TOP_Y, LCD_W, BODY_H }

/* Shared chrome widgets â€” defined in meletweaks.c */
extern GfxWidget      *s_navbar;
extern GfxWidget      *s_statusbar;
extern GfxRenderTarget s_overlay_bg_target;

/* Factories â€” defined in meletweaks.c */
GfxWidget *make_border(void);
GfxWidget *make_menu(const GfxMenuItem *items, int n, GfxMenuIndicatorAlign align);
GfxWidget *make_placeholder(const char *text);

/* Shared appliers â€” defined in meletweaks.c */
void accent_menu(GfxWidget *w, GfxColor c);
void bg_menu    (GfxWidget *w, GfxColor c);
void bg_clock   (GfxWidget *w, GfxColor c);   /* used by overlay_bg + screen_clock */
void secondary_clock(GfxWidget *w, GfxColor c); /* same */

/* Called from screen_settings to trigger sleep or toggle FPS overlay */
void meletweaks_sleep_now(void);
int  fps_show_get(void);
void fps_show_set(int v);
int  input_inverted_get(void);
void input_inverted_set(int v);

#endif /* SCREENS_CHROME_H */
