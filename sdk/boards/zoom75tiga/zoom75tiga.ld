PSRAM_ADDR = 0x220e0000;

/* Firmware-internal functions (Thumb LSB included). */
m_draw_frame = 0x10019E61;   /* void m_draw_frame(const u8 *fb) */
wd_refresh   = 0x1001E26D;   /* void wd_refresh(void)           */

INCLUDE fr8000.ld
