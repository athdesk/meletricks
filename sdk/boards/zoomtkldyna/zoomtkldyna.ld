/* PSRAM user-image window ends at 0x22200000 (1.125 MB). */
PSRAM_ADDR = 0x220e0000;

/* Firmware-internal functions (Thumb LSB included). */
m_draw_frame = 0x1001A115;   /* void m_draw_frame(const u8 *fb) */
wd_refresh   = 0x1001E785;   /* void wd_refresh(void)           */

INCLUDE fr8000.ld
