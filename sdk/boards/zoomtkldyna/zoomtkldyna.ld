/* PSRAM user-image window ends at 0x22200000 (1.125 MB). */
PSRAM_ADDR = 0x220e0000;

/* Firmware-internal functions (Thumb LSB included). */
m_draw_frame = 0x1001A115;   /* void m_draw_frame(const u8 *fb) */
wd_refresh   = 0x1001E785;   /* void wd_refresh(void)           */
fw_rtc_set   = 0x1001DE19;   /* u32  fw_rtc_set(u32 packed)     */

INCLUDE fr8000.ld
