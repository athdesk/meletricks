#include "firmware.h"
#include "zoomtkldyna.h"

#define THUMB_BX_LR 0x4770u

/* Thin wrappers around board-supplied firmware function pointers.
 * Keeping these as real C functions (rather than inlining at every call
 * site) preserves the `m_draw_frame` / `wd_refresh` / `fw_rtc_set` symbol
 * names in the linked ELF — tweakloader looks `fw_rtc_set` up by name
 * over BLE, and `wd_refresh` is taken via weak ref in sdk_dispatch.c. */
void m_draw_frame(const u8 *fb) { FW_DRAW_FRAME_FN(fb); }
void wd_refresh(void)           { FW_WD_REFRESH_FN(); }
u32  fw_rtc_set(u32 packed)     { return FW_RTC_SET_FN(packed); }

static u16 saved_instr;

/* Patch the first instruction of the firmware GUI task with BX LR so each
 * invocation returns immediately; restore on resume.
 * TODO: Consider adding a refcount here to avoid deleting the saved instruction */
void gui_pause(void)
{
    saved_instr = *FW_GUI_TASK_HANDLER;
    *FW_GUI_TASK_HANDLER = THUMB_BX_LR;
}

void gui_resume(void)
{
    *FW_GUI_TASK_HANDLER = saved_instr;
}

/* FW_LCD_TE_GATE: basically VSYNC enabled flag */
void lcd_te_sync_disable(void) { *FW_LCD_TE_GATE = 0; }
void lcd_te_sync_enable (void) { *FW_LCD_TE_GATE = 1; }

/* FW_LCD_IDLE_FLAG: low byte of the GC9C01 SPI state struct. Cleared to 0
 * by gc9c01_display when it kicks a DMA; the DMA-complete IRQ sets it to 1.
 * TODO: consider yielding instead of spinning; need to understand the consequences.
 */
int  lcd_is_idle(void)   { return *FW_LCD_IDLE_FLAG != 0; }
void lcd_wait_idle(void) { while (!*FW_LCD_IDLE_FLAG) { /* spin */ } }

/* FW_RTC_*: g_rtc_datetime_ymd @ 0x11003A24, g_rtc_datetime_hms @ 0x11003A28.
 * Updated every second by sys_tick_1s_handler -> rtc_second_tick. */
void lcd_rtc_get_live(lcd_rtc_live_t *out)
{
    out->year   = *FW_RTC_YEAR;
    out->month  = *FW_RTC_MONTH;
    out->day    = *FW_RTC_DAY;
    out->hour   = *FW_RTC_HOUR;
    out->minute = *FW_RTC_MINUTE;
    out->second = *FW_RTC_SECOND;
}

/* FW_DISPLAY_SLEEP_FN / FW_DISPLAY_WAKE_FN: display_check_and_sleep /
 * display_check_and_wake — same path UART opcode 0x32 takes.
 *   sleep: DISPOFF + 50ms + SLPIN + 120ms
 *   wake:  SLPOUT + 50ms + DISPON + 120ms
 *
 * FW_LCD_FULL_REINIT_ARMED: set this to 1 to prevent firmware to redo
 * initialization after waking up LCD. it crashes us otherwise
 *
 * FW_LCD_PREVENT_SLEEP_BIT: rwip prevent-sleep mask bit held while panel is
 * off. The firmware releases its own lock (bit 0x100) ~10 s after sleep,
 * which would let the SoC deep-sleep and trash injected code on wake. */

// Is it worth to refcount those? For now we assume the caller fully owns sleep state.
void lcd_sleep(void)
{
    rwip_prevent_sleep_set(FW_LCD_PREVENT_SLEEP_BIT);
    *FW_LCD_FULL_REINIT_ARMED = 1;
    FW_DISPLAY_SLEEP_FN();
}

void lcd_wake(void)
{
    FW_DISPLAY_WAKE_FN();
    *FW_LCD_FULL_REINIT_ARMED = 0;
    rwip_prevent_sleep_clear(FW_LCD_PREVENT_SLEEP_BIT);
}
