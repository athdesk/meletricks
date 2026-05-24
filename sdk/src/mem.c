#include "fr8000.h"
#include "fr_types.h"
#include "mem.h"

void *memcpy(void *dest, const void *src, u32 n)
{
    __aeabi_memcpy(dest, src, n);
    return dest;
}

void *memset(void *dest, int c, u32 n)
{
    /* AEABI memset swaps n / c vs the C lib signature. */
    __aeabi_memset(dest, n, c);
    return dest;
}
