/*
 * Per-tick policy.  Called from sdk_entry (entry.s) once per
 * firmware tick.  Owns: first-tick SDK + FR_SETUP init, SDK
 * housekeeping, watchdog kick in fw-override mode, the task walk, and
 * stack-overflow detection.
 *
 * SDK state lives in `g_sdk` (struct below).  entry.s reaches into
 * the same struct via hardcoded `.equ` offsets; the _Static_assert
 * block here is the contract those offsets are checked against —
 * touch field order or sizes here and the build fails if the asm
 * wasn't updated to match.
 *
 * Per-task coroutine state lives in each task's sdk_task_t entry in
 * .fr_user_tasks_table (offsets also static-asserted below).
 *
 * Stack overflow: each task gets a 4-byte canary written at the
 * bottom of its stack on the first tick.  Before re-entering a task
 * each tick, the scheduler checks the canary; mismatch -> overflow.
 * Detection halts ALL tasks (g_sdk.tasks_halted = 1) because the
 * offender's underflow may have trashed neighbour state in .bss;
 * the event is recorded in g_sdk_panic for host inspection.
 */

#include <stddef.h>
#include "fr_types.h"
#include "entry.h"

#ifndef SDK_DISABLE_KBD_EVENTS
#include "kbd_event.h"
#endif

extern u8 _fr_user_setup_start[];
extern u8 _fr_user_setup_end[];

extern sdk_task_t _fr_user_tasks_table_start[];
extern sdk_task_t _fr_user_tasks_table_end[];

/* In entry.s.  Either runs `task->entry` on `task->stack_base` from
 * the top, or resumes from a prior fr_yield.  Returns when the task
 * either completes or yields. */
extern void sdk_enter_or_resume(sdk_task_t *task);

typedef void (*fr_fn_t)(void);


/* ── SDK state ────────────────────────────────────────────────────── */

typedef struct {
    void *trampoline_ret;       /*  +0  saved lr on entry           */
    void *firmware_sp;          /*  +4  saved firmware sp           */
    void *scheduler_sp;         /*  +8  sp at sdk_enter_or_resume   */
    void *scheduler_lr;         /* +12  lr at sdk_enter_or_resume   */
    void *current_task;         /* +16  sdk_task_t* of running task */
    u8    setup_done;           /* +20  first-tick gate             */
    u8    fw_override;          /* +21  sdk_set_fw_override flag    */
    u8    tasks_halted;         /* +22  set by sdk_panic; loop skips */
} sdk_state_t;

_Static_assert(offsetof(sdk_state_t, trampoline_ret) == 0,  "SDK_TRAMPOLINE_RET offset");
_Static_assert(offsetof(sdk_state_t, firmware_sp)    == 4,  "SDK_FIRMWARE_SP offset");
_Static_assert(offsetof(sdk_state_t, scheduler_sp)   == 8,  "SDK_SCHEDULER_SP offset");
_Static_assert(offsetof(sdk_state_t, scheduler_lr)   == 12, "SDK_SCHEDULER_LR offset");
_Static_assert(offsetof(sdk_state_t, current_task)   == 16, "SDK_CURRENT_TASK offset");
_Static_assert(offsetof(sdk_state_t, setup_done)     == 20, "SDK_SETUP_DONE offset");
_Static_assert(offsetof(sdk_state_t, fw_override)    == 21, "SDK_FW_OVERRIDE offset");
_Static_assert(offsetof(sdk_state_t, tasks_halted)   == 22, "SDK_TASKS_HALTED offset");

/* Per-task layout — must match the .equ block in entry.s. */
_Static_assert(offsetof(sdk_task_t, entry)       == 0,  "TASK_ENTRY offset");
_Static_assert(offsetof(sdk_task_t, stack_base)  == 4,  "TASK_STACK_BASE offset");
_Static_assert(offsetof(sdk_task_t, stack_size)  == 8,  "TASK_STACK_SIZE offset");
_Static_assert(offsetof(sdk_task_t, saved_sp)    == 12, "TASK_SAVED_SP offset");
_Static_assert(offsetof(sdk_task_t, resume_lr)   == 16, "TASK_RESUME_LR offset");
_Static_assert(offsetof(sdk_task_t, saved_regs)  == 20, "TASK_SAVED_REGS offset");
_Static_assert(offsetof(sdk_task_t, suspended)   == 52, "TASK_SUSPENDED offset");

volatile sdk_state_t g_sdk;


/* ── Panic reporting ──────────────────────────────────────────────── */

#define SDK_PANIC_NONE             0u
#define SDK_PANIC_STACK_OVERFLOW   1u

typedef struct {
    u32         reason;         /* SDK_PANIC_* */
    sdk_task_t *task;           /* offender, or NULL */
    u32         count;          /* total panics observed */
} sdk_panic_t;

volatile sdk_panic_t g_sdk_panic;

static void sdk_panic(u32 reason, sdk_task_t *task)
{
    g_sdk_panic.reason = reason;
    g_sdk_panic.task   = task;
    g_sdk_panic.count++;
    g_sdk.tasks_halted = 1;
}


void sdk_set_fw_override(int enabled) { g_sdk.fw_override = enabled ? 1u : 0u; }
int  sdk_get_fw_override(void)        { return g_sdk.fw_override; }

extern void wd_refresh(void) __attribute__((weak));


static void sdk_system_setup_once(void)
{
#ifndef SDK_DISABLE_KBD_EVENTS
    kbd_event_init();
#endif
}

static void sdk_system_loop(void)
{
    /* Hook for future per-tick SDK work. */
}


/* Write the stack canary at the bottom of every task's stack.  Done
 * once on first tick, before any task runs. */
static void sdk_canaries_init(void)
{
    for (sdk_task_t *t = _fr_user_tasks_table_start;
         t < _fr_user_tasks_table_end; t++) {
        *(volatile u32 *)t->stack_base = SDK_STACK_CANARY;
    }
}


static void sdk_first_tick(void)
{
    sdk_system_setup_once();
    sdk_canaries_init();
    if (&_fr_user_setup_end[0] > &_fr_user_setup_start[0]) {
        fr_fn_t setup = (fr_fn_t)((u32)_fr_user_setup_start | 1u);
        setup();
    }
    g_sdk.setup_done = 1;
}

void sdk_run_tick(void)
{
    if (!g_sdk.setup_done) sdk_first_tick();

    do {
        sdk_system_loop();
        if (g_sdk.fw_override && wd_refresh) wd_refresh();
        if (!g_sdk.tasks_halted) {
            for (sdk_task_t *t = _fr_user_tasks_table_start;
                 t < _fr_user_tasks_table_end; t++) {
                if (*(volatile u32 *)t->stack_base != SDK_STACK_CANARY) {
                    sdk_panic(SDK_PANIC_STACK_OVERFLOW, t);
                    break;
                }
                sdk_enter_or_resume(t);
            }
        }

        // If fw_override is set, we keep looping here to avoid returning to
        // the point where we have hooked in the real firmware.
        // Once fw_override is cleared, we will return to the real firmware,
        // and we expect to be called again on the next tick.
    } while (g_sdk.fw_override);
}
