#include "fr8000.h"
#include "fr_types.h"
#include "timer.h"
#include "entry.h"

u32 fr_micros(void)
{
    fr_time_t t;
    rwip_time_get(&t);
    return (u32)(((u64)t.hs * 625u + t.hus) / 2u);
}

void fr_delay_us(u32 us)
{
    u32 start = fr_micros();
    while ((u32)(fr_micros() - start) < us)
        fr_yield();
}

/* hs is 312.5 us = 5/16 ms; u64 avoids overflow, result wraps ~15.5 days.
 * We're not wrapping micros functions to get a bit more wraparound headroom.
 */
u32 fr_millis(void)
{
    fr_time_t t;
    rwip_time_get(&t);
    return (u32)((u64)t.hs * 5 / 16);
}

void fr_delay_ms(u32 ms)
{
    u32 start = fr_millis();
    while ((u32)(fr_millis() - start) < ms)
        fr_yield();
}

void fr_sleep_us(u32 us)
{
    u32 start = fr_micros();
    while ((u32)(fr_micros() - start) < us) {}
}

void fr_sleep_ms(u32 ms)
{
    u32 start = fr_millis();
    while ((u32)(fr_millis() - start) < ms) {}
}
