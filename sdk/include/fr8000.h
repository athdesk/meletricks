/*
 * Typed declarations for fr8000 ROM syscalls.
 *
 * All ROM symbols are provided by fr8000_syscalls.ld; only verified
 * signatures live here. To use a syscall not listed below, declare
 * it locally; promote here once the signature is confirmed.
 *
 */
#ifndef FR8000_H
#define FR8000_H

#include "fr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ROM is at 0x0 -> 0x1ffff; this code may live further out, outside
 * 16 MB range of any BL.  long_call forces ldr/blx */
#define FR_SYSCALL __attribute__((long_call))

/* -- rwip (BLE scheduler clock) ------------------------------------- */

FR_SYSCALL void rwip_time_get(fr_time_t *time);
FR_SYSCALL void rwip_sleep_mul_64(u32 *out_lo, u32 *out_hi, u32 a, u32 b);

/* Prevent-sleep bitmap at SRAM 0x11000B9E. set ORs the mask, clear
 * AND-NOTs it; pass a caller-specific bit so multiple subsystems can
 * block sleep independently. TODO: catalogue the bits the firmware uses */
FR_SYSCALL void rwip_prevent_sleep_set(u16 prevent_mask);
FR_SYSCALL void rwip_prevent_sleep_clear(u16 prevent_mask);

FR_SYSCALL u32 rwip_lpcycles_2_hus(u32 lpcycles);
FR_SYSCALL u32 rwip_us_2_lpcycles(u32 us);
FR_SYSCALL u32 rwip_slot_2_lpcycles_rom(u32 slots);


/* -- AEABI memory ops ----------------------------------------------- */

FR_SYSCALL void __aeabi_memcpy(void *dest, const void *src, u32 n);
FR_SYSCALL void __aeabi_memcpy4(void *dest, const void *src, u32 n);
FR_SYSCALL void __aeabi_memcpy8(void *dest, const void *src, u32 n);
FR_SYSCALL void __aeabi_memset(void *dest, u32 n, int c);
FR_SYSCALL void __aeabi_memset4(void *dest, u32 n, int c);
FR_SYSCALL void __aeabi_memset8(void *dest, u32 n, int c);
FR_SYSCALL void __aeabi_memclr(void *dest, u32 n);
FR_SYSCALL void __aeabi_memclr4(void *dest, u32 n);
FR_SYSCALL void __aeabi_memclr8(void *dest, u32 n);

FR_SYSCALL int memcmp(const void *s1, const void *s2, u32 n);


/* -- KE memory allocator -------------------------------------------- */
/* 4 pools indexed 0..3; `type` selects which.  ke_get_mem_usage and
 * ke_mem_init each act on a single pool.
 * This should be used carefully as we don't want to starve the firmware
 * (seems to reset on OOM)
 */

FR_SYSCALL void *ke_malloc(u32 size, u8 type);
FR_SYSCALL void  ke_free(void *ptr);
FR_SYSCALL void  ke_mem_init(u8 pool_idx, void *base, u32 size);
FR_SYSCALL u16   ke_get_mem_usage(u8 pool_idx);
FR_SYSCALL bool  ke_is_free(void *ptr);


/* -- RNG ------------------------------------------------------------ */

FR_SYSCALL void srand(u32 seed);
FR_SYSCALL int  rand(void);


/* -- GPIO ----------------------------------------------------------- */
/* Ports: 0=PA, 1=PB, 2=PC, 3=PD, 4=PE.  MMIO base differs A/B vs C/D/E.
 * gpio_get_pin_value takes pin as a pre-computed BITMASK in r2 (not
 * a pin number); the middle arg is consumed by an internal base load
 * and otherwise unused. */

FR_SYSCALL void gpio_set_dir(u8 port, u8 pin, u32 dir);
FR_SYSCALL void gpio_set_pin_value(u8 port, u8 pin, u32 value);
FR_SYSCALL int  gpio_get_pin_value(u8 port, u32 _unused, u32 pin_mask);


/* -- Flash ---------------------------------------------------------- */
/* flash_read / flash_write take (addr, LEN, BUF) — buffer LAST.
 * flash_read_id returns the 24-bit JEDEC id directly;
 * flash_read_status returns 16-bit when read_high != 0. */

FR_SYSCALL void flash_init(void);
FR_SYSCALL void flash_read(u32 addr, u32 len, void *buf);
FR_SYSCALL void flash_write(u32 addr, u32 len, const void *buf);
FR_SYSCALL void flash_erase(u32 addr, u32 len);
FR_SYSCALL void flash_chip_erase(void);
FR_SYSCALL u32  flash_read_id(void);
FR_SYSCALL u16  flash_read_status(int read_high);
FR_SYSCALL void flash_write_status(u16 status, int write_high);


/* -- System --------------------------------------------------------- */

FR_SYSCALL void platform_reset(u32 error);
FR_SYSCALL void system_init(void);
FR_SYSCALL u32  system_get_pclk(void);
FR_SYSCALL void system_set_pclk(u32 clk);

FR_SYSCALL u32 get_stack_limit(void);
FR_SYSCALL u32 get_stack_usage(void);

FR_SYSCALL void co_delay_10us(u32 count);
FR_SYSCALL void co_delay_100us(u32 count);

FR_SYSCALL u32 crc32(u32 crc, const u8 *buf, u32 len);

#ifdef __cplusplus
}
#endif

#endif /* FR8000_H */
