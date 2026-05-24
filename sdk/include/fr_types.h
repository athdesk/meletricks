#ifndef FR_TYPES_H
#define FR_TYPES_H

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;

typedef unsigned int        uint;
typedef int                 bool;

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Layout must match the ROM's rwip_time_get output parameter. */
typedef struct
{
    u32 hs;   /* half-slot count (312.5 us per half-slot) */
    u32 hus;  /* half-microsecond offset within slot (0..624) */
} fr_time_t;

#endif /* FR_TYPES_H */
