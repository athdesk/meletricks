/*
 * Task entry + scheduler API.  Pairs with entry.s + sdk_dispatch.c.
 *
 *   FR_TASK(name, stack_size) { ... }    // declare a per-tick task
 *   FR_SETUP void boot(void)  { ... }    // optional, at most one
 *
 * Each FR_TASK reserves a private stack and gets called once per
 * firmware tick.  Tasks may fr_yield() — that suspends just that
 * task; other tasks still run on the same tick.  The suspended task
 * resumes from its yield point on the next tick.
 *
 * At least one FR_TASK must exist (enforced by a linker ASSERT).
 * FR_SETUP is optional; if present it runs once on the first tick,
 * before any task.
 *
 * NOTE: support for more than one FR_TASK is yet untested.
 */
#ifndef ENTRY_H
#define ENTRY_H

#include "fr_types.h"

/* -- Task descriptor -------------------------------------------------
 *
 * The FR_TASK macro emits one of these into .fr_user_tasks_table.
 * Layout is part of the asm contract — sdk_dispatch.c has
 * _Static_assert guards that fail the build if anything drifts. */

typedef struct {
    void (*entry)(void);        /* +0  */
    u8   *stack_base;           /* +4  */
    u32   stack_size;           /* +8  */
    /* runtime, set by entry.s / fr_yield */
    void *saved_sp;             /* +12 */
    void *resume_lr;            /* +16 */
    u32   saved_regs[8];        /* +20 r4..r11 */
    u8    suspended;            /* +52 */
} sdk_task_t;

/* Canary written at stack_base[0..3] on first tick; checked before each
 * task entry. In case one stack gets overflowed we pause every task to
 * allow reading the panic report using firmware BLE functionality */
#define SDK_STACK_CANARY 0x7374616bu  /* 'stak' */


/* -- Tag macros ---------------------------------------------------- */

#define _FR_PASTE(a, b) a##b
#define _FR_PASTE_X(a, b) _FR_PASTE(a, b)

/* FR_TASK(name, sz):
 * sz is the stack size in bytes
 *
 * Use as a function-definition header:
 *
 *     FR_TASK(my_tick, 1024)
 *     {
 *         ... body ...
 *     }
 */
#define FR_TASK(name, stack_sz)                                            \
    static void name(void);                                                \
    static u8 _FR_PASTE_X(_fr_stack_, name)[stack_sz]                      \
        __attribute__((aligned(8)));                                       \
    static sdk_task_t _FR_PASTE_X(_fr_task_, name)                         \
        __attribute__((used, section(".fr_user_tasks_table"))) = {         \
            .entry      = name,                                            \
            .stack_base = _FR_PASTE_X(_fr_stack_, name),                   \
            .stack_size = stack_sz,                                        \
        };                                                                 \
    static void name(void)

/* FR_SETUP marker is enforced as <= 1 by the linker (see fr8000.ld). */
#define _FR_EMIT_MARKER(section_name)                                      \
    static const char _FR_PASTE_X(_fr_marker_, __COUNTER__)                \
        __attribute__((used, section(section_name))) = 1

#define FR_SETUP                                                           \
    _FR_EMIT_MARKER(".fr_user_setup_marker");                              \
    __attribute__((used, section(".fr_user_setup")))


/* -- Coroutine + scheduling API ------------------------------------ */

/* Yield to the scheduler — suspends this task until the next tick,
 * letting other tasks run. */
void fr_yield(void);

/* Firmware-override mode: spin the task loop without returning to firmware
 * between ticks. The SDK kicks the watchdog on every iteration.
 * fr_yield still suspends normally and gives other tasks their slice.
 * Override only affects whether firmware gets control back between ticks. */
void sdk_set_fw_override(int enabled);
int  sdk_get_fw_override(void);

#endif /* ENTRY_H */
