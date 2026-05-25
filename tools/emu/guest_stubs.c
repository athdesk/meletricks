/*
 * guest_stubs.c — compiled INTO the guest ELF.
 *
 * Provides:
 *   - libc-ish helpers gcc emits implicit calls to (memcpy/memset/memcmp
 *     and the AEABI variants).  On-device these are ROM thunks; here we
 *     just give the linker real Thumb code so the host doesn't have to
 *     trap on every implicit struct copy.
 *   - _emu_entry: a no-op the ENTRY directive points at.  The host never
 *     actually executes this; it sets PC directly to hello_setup /
 *     hello_loop addresses pulled from the ELF symbol table.
 */

#include "fr_types.h"

void *memcpy(void *dst, const void *src, u32 n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, u32 n)
{
    u8 *d = (u8 *)dst;
    while (n--) *d++ = (u8)c;
    return dst;
}

int memcmp(const void *a, const void *b, u32 n)
{
    const u8 *p = a, *q = b;
    while (n--) { if (*p != *q) return *p - *q; p++; q++; }
    return 0;
}

/* AEABI variants — gcc emits these for aggregate copies on -mthumb. */
void __aeabi_memcpy (void *dst, const void *src, u32 n) { memcpy(dst, src, n); }
void __aeabi_memcpy4(void *dst, const void *src, u32 n) { memcpy(dst, src, n); }
void __aeabi_memcpy8(void *dst, const void *src, u32 n) { memcpy(dst, src, n); }

/* AEABI memset has swapped arg order vs libc. */
void __aeabi_memset (void *dst, u32 n, int c) { memset(dst, c, n); }
void __aeabi_memset4(void *dst, u32 n, int c) { memset(dst, c, n); }
void __aeabi_memset8(void *dst, u32 n, int c) { memset(dst, c, n); }

void __aeabi_memclr (void *dst, u32 n) { memset(dst, 0, n); }
void __aeabi_memclr4(void *dst, u32 n) { memset(dst, 0, n); }
void __aeabi_memclr8(void *dst, u32 n) { memset(dst, 0, n); }

__attribute__((section(".emu_entry"), used, naked))
void _emu_entry(void) { __asm__("bx lr"); }
