#ifndef COMMON_H
#define COMMON_H

#include "entry.h"
#include "firmware.h"
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

#define HEADER_BC_X     14
#define HEADER_BC_W     212
#define HEADER_BATT_X   232
#define HEADER_BATT_W   74


// We use these for Wireframe and Bounce to get less padding
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
