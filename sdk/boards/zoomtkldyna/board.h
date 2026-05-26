#ifndef BOARD_H
#define BOARD_H

#include "fr_types.h"

#define FW_GUI_TASK_HANDLER      ((volatile u16 *)0x11001082u)

#define FW_LCD_TE_GATE           ((volatile u8  *)0x11003A71u)
#define FW_LCD_IDLE_FLAG         ((volatile u8  *)0x11003A70u)

#define FW_RTC_YEAR              ((volatile u16 *)0x11003A24u)
#define FW_RTC_MONTH             ((volatile u8  *)0x11003A26u)
#define FW_RTC_DAY               ((volatile u8  *)0x11003A27u)
#define FW_RTC_SECOND            ((volatile u8  *)0x11003A28u)
#define FW_RTC_MINUTE            ((volatile u8  *)0x11003A29u)
#define FW_RTC_HOUR              ((volatile u8  *)0x11003A2Au)

#define FW_DISPLAY_SLEEP_FN      ((void (*)(void))(0x1001DF04u | 1u))
#define FW_DISPLAY_WAKE_FN       ((void (*)(void))(0x1001DF20u | 1u))

#define FW_DRAW_FRAME_FN         ((void (*)(const u8 *))(0x1001A114u | 1u))
#define FW_WD_REFRESH_FN         ((void (*)(void))      (0x1001E784u | 1u))
#define FW_RTC_SET_FN            ((u32  (*)(u32))       (0x1001DE18u | 1u))

#define FW_LCD_FULL_REINIT_ARMED ((volatile u8  *)0x11003974u)
#define FW_LCD_PREVENT_SLEEP_BIT 0x4000u

#define FW_TICK_HOOK             0x110014EC

#endif /* BOARD_H */
