/*
 * firmware.h — board-generic API for the LCD firmware hooks.
 */

#ifndef FIRMWARE_H
#define FIRMWARE_H

#include "fr8000.h"
#include "fr_types.h"
#include "uart_irq.h"   /* re-export: install_uart_isr + UART0_* macros */

/* Triggers a full DMA refresh of the 320x172 RGB565 LCD framebuffer.  The
 * source buffer must be exactly that geometry
 * Partial blits would require using set_window. We don't support it yet. */
void m_draw_frame(const u8 *fb);

/* Kick the firmware watchdog timer. */
void wd_refresh(void);

/* Decode a packed datetime word and store it in the firmware's RTC. */
u32  fw_rtc_set(u32 packed);

/* Silence / restore the firmware's GUI task so its periodic redraws don't
 * fight us for the LCD framebuffer.  Implemented by patching the first
 * instruction of the task with BX LR and restoring it on resume. */
void gui_pause(void);
void gui_resume(void);

/* Disable the VSYNC wait in m_draw_frame. The firmware busy-waits, we save some time */
void lcd_te_sync_disable(void);
void lcd_te_sync_enable(void);

/* LCD DMA idle gate.
 *
 * `gc9c01_display` is async: it sets a "busy" flag, fires the DMA, and returns
 * the DMA-complete IRQ clears the flag.
 * Calling `m_draw_frame` while busy is silently dropped
 *
 */
int  lcd_is_idle(void);
void lcd_wait_idle(void);

/* Live broken-down RTC read from the firmware's free-running clock. */
typedef struct {
    u16 year;       /* full year, e.g. 2026 */
    u8  month;      /* 1..12 */
    u8  day;        /* 1..31 */
    u8  hour;       /* 0..23 */
    u8  minute;     /* 0..59 */
    u8  second;     /* 0..59 */
} lcd_rtc_live_t;

void lcd_rtc_get_live(lcd_rtc_live_t *out);

/* Screen sleep / wake. */
void lcd_sleep(void);
void lcd_wake(void);

#endif /* FIRMWARE_H */
