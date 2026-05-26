#ifndef BOARD_H
#define BOARD_H

#include "fr_types.h"

#define FW_GUI_TASK_HANDLER      ((volatile u16 *)0xDEAD0000u)

#define FW_LCD_TE_GATE           ((volatile u8  *)0xDEAD0010u)
#define FW_LCD_IDLE_FLAG         ((volatile u8  *)0xDEAD0011u)

#define FW_RTC_YEAR              ((volatile u16 *)0xDEAD0020u)
#define FW_RTC_MONTH             ((volatile u8  *)0xDEAD0022u)
#define FW_RTC_DAY               ((volatile u8  *)0xDEAD0023u)
#define FW_RTC_SECOND            ((volatile u8  *)0xDEAD0024u)
#define FW_RTC_MINUTE            ((volatile u8  *)0xDEAD0025u)
#define FW_RTC_HOUR              ((volatile u8  *)0xDEAD0026u)

#define FW_DISPLAY_SLEEP_FN      ((void (*)(void))(0xDEAD0030u | 1u))
#define FW_DISPLAY_WAKE_FN       ((void (*)(void))(0xDEAD0034u | 1u))

#define FW_DRAW_FRAME_FN         ((void (*)(const u8 *))(0xDEAD0040u | 1u))
#define FW_WD_REFRESH_FN         ((void (*)(void))      (0xDEAD0044u | 1u))
#define FW_RTC_SET_FN            ((u32  (*)(u32))       (0xDEAD0048u | 1u))

#define FW_LCD_FULL_REINIT_ARMED ((volatile u8  *)0xDEAD0050u)
#define FW_LCD_PREVENT_SLEEP_BIT 0x4000u

#endif
