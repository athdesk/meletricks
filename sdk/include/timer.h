/*
 * Timers + delays.  Pairs with timer.c.
 *
 * Counters wrap at the u32 boundary (~71 minutes for fr_micros,
 * ~49 days for fr_millis).
 *
 * fr_delay_*  — yield between polls; other SDK tasks run while waiting.
 * fr_sleep_*  — busy-wait with no yield.  WARNING: long sleeps will
 *               trigger the firmware watchdog.  Use only for short
 *               durations (tens of microseconds) where yielding would
 *               introduce unacceptable latency.
 */
#ifndef TIMER_H
#define TIMER_H

#include "fr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

u32  fr_micros(void);
void fr_delay_us(u32 us);
void fr_sleep_us(u32 us);

u32  fr_millis(void);
void fr_delay_ms(u32 ms);
void fr_sleep_ms(u32 ms);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H */
