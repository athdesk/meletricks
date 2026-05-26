#ifndef ZOOM75TIGA_H
#define ZOOM75TIGA_H

#include "fr_types.h"

#define FW_GUI_TASK_HANDLER      ((volatile u16 *)0x11001082u)

#define FW_LCD_TE_GATE           ((volatile u8  *)0x11003A2Du)
#define FW_LCD_IDLE_FLAG         ((volatile u8  *)0x11003A2Cu)

#define FW_RTC_YEAR              ((volatile u16 *)0x110039F4u)
#define FW_RTC_MONTH             ((volatile u8  *)0x110039F6u)
#define FW_RTC_DAY               ((volatile u8  *)0x110039F7u)
#define FW_RTC_SECOND            ((volatile u8  *)0x110039F8u)
#define FW_RTC_MINUTE            ((volatile u8  *)0x110039F9u)
#define FW_RTC_HOUR              ((volatile u8  *)0x110039FAu)

#define FW_DISPLAY_SLEEP_FN      ((void (*)(void))(0x1001DA58u | 1u))
#define FW_DISPLAY_WAKE_FN       ((void (*)(void))(0x1001DA74u | 1u))

#define FW_DRAW_FRAME_FN         ((void (*)(const u8 *))(0x10019E60u | 1u))
#define FW_WD_REFRESH_FN         ((void (*)(void))      (0x1001E26Cu | 1u))
#define FW_RTC_SET_FN            ((u32  (*)(u32))       (0x1001D96Cu | 1u))

#define FW_LCD_FULL_REINIT_ARMED ((volatile u8  *)0x11003940u)
#define FW_LCD_PREVENT_SLEEP_BIT 0x4000u

#endif /* ZOOM75TIGA_H */
