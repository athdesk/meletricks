/*
 * C-standard memcpy / memset.  Pairs with mem.c, which forwards to
 * the AEABI ROM impls (with the argument order back to libc-style).
 */
#ifndef MEM_H
#define MEM_H

#include "fr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *dest, const void *src, u32 n);
void *memset(void *dest, int c, u32 n);

#ifdef __cplusplus
}
#endif

#endif /* MEM_H */
