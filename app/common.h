/* Shared layout constants, font externs, and the current-accent
 * accessor that lets widgets pick up the foreground colour without
 * pulling in the settings module. */
#ifndef HELLO_COMMON_H
#define HELLO_COMMON_H

#include "zoomtkldyna.h"
#include "entry.h"
#include "kbd_event.h"
#include "timer.h"
#include "fr8000.h"
#include "gfx.h"

#define LCD_W   320
#define LCD_H   172
#define LCD_FB  ((u16 *)0x22000000u)

#define BODY_INSET_X  14
#define BODY_TOP_Y    42
#define BODY_H        (LCD_H - 78)

/* Header strip: breadcrumb on the left fills the bar, battery widget
 * pinned in the right corner. WPM + FPS live down in the statusbar
 * now (see STATUSBAR_* below). */
#define HEADER_BC_X     14
#define HEADER_BC_W     212
#define HEADER_BATT_X   232
#define HEADER_BATT_W   74


/* Wireframe and Bounce extend almost the full available vertical
 * range — overlay-bg-backed demos look better at the larger size,
 * and they don't run into Life-style cell-grid math that depends
 * on BODY_H. */
#define EXPANDED_BODY_TOP_Y 30
#define EXPANDED_BODY_H     104

extern const GfxFont font_montserrat_14;
extern const GfxFont font_montserrat_24;
extern const GfxFont font_montserrat_64;
extern const GfxFont font_lora_14;
extern const GfxFont font_lora_24;
extern const GfxFont font_fira_mono_14;
extern const GfxFont font_fira_mono_24;
extern const GfxFont font_barlow_condensed_14;
extern const GfxFont font_barlow_condensed_24;

#endif
