@ sdk_entry / sdk_enter_or_resume / fr_yield —
@ entry stub + per-task coroutine primitives.
@
@ Scheduling policy lives in sdk_run_tick (sdk_dispatch.c).  All SDK
@ state lives in `g_sdk` (sdk_state_t).
@
@ Per-task coroutine state lives in each task's sdk_task_t entry.
@ sdk_dispatch.c has _Static_assert guards that fail the build if struct shape drifts.
@
@ Stacks:
@   firmware stack — owned by the firmware, saved on entry to sdk_entry.
@   scheduler stack — g_scheduler_stack, sdk_run_tick runs here.
@   task stacks — one per FR_TASK, allocated in the task's TU.
@
@ Our entry point will be called by a small shellcode which basically hooks
@ another function that is run on every firmware tick.
@ For this reason, we must not corrupt previous state:
@ registers are pushed on the stack, and they get popped after we return
@ we must restore sp before we bx lr back to the trampoline.

.syntax unified
.cpu cortex-m3
.thumb

@ sdk_run_tick + sdk_first_tick (incl. kbd_event_init + FR_SETUP) all
@ run on this stack.  4 KB is plenty (maybe even excessive) for setup work
.equ SCHEDULER_STACK_SIZE, 4096

@ g_sdk field offsets.
.equ SDK_TRAMPOLINE_RET, 0
.equ SDK_FIRMWARE_SP,    4
.equ SDK_SCHEDULER_SP,   8
.equ SDK_SCHEDULER_LR,   12
.equ SDK_CURRENT_TASK,   16
.equ SDK_SETUP_DONE,     20
.equ SDK_FW_OVERRIDE,    21

@ sdk_task_t field offsets.
.equ TASK_ENTRY,         0
.equ TASK_STACK_BASE,    4
.equ TASK_STACK_SIZE,    8
.equ TASK_SAVED_SP,      12
.equ TASK_RESUME_LR,     16
.equ TASK_SAVED_REGS,    20    @ r4..r11, 32 bytes
.equ TASK_SUSPENDED,     52


.section .bss
.align 3
g_scheduler_stack:       .space SCHEDULER_STACK_SIZE
g_scheduler_stack_top:                            @ one past end

.section .entry, "ax", %progbits
.align 2
.global sdk_entry
.type   sdk_entry, %function
.thumb_func
sdk_entry:
    ldr  r3, =g_sdk
    str  lr, [r3, #SDK_TRAMPOLINE_RET]
    str  sp, [r3, #SDK_FIRMWARE_SP]

    ldr  r0, =g_scheduler_stack_top
    mov  sp, r0
    bl   sdk_run_tick

    ldr  r3, =g_sdk
    ldr  r0, [r3, #SDK_FIRMWARE_SP]
    mov  sp, r0
    ldr  lr, [r3, #SDK_TRAMPOLINE_RET]
    bx   lr

    .ltorg
.size sdk_entry, . - sdk_entry


.section .text
.align 2

@ void sdk_enter_or_resume(sdk_task_t *task)
@
@ Stash scheduler sp/lr in g_sdk (fr_yield reads them to return here),
@ then either resume the task from its saved coroutine state or start
@ it fresh on its private stack (based on suspended flag).
@
@ Two ways to land back in the caller:
@   1. Task returns naturally — falls through to .Lreturn_to_sched.
@   2. Task calls fr_yield — fr_yield reads scheduler_sp/lr and bx's
@      directly to the caller, bypassing the rest of this function.

.global sdk_enter_or_resume
.type   sdk_enter_or_resume, %function
.thumb_func
sdk_enter_or_resume:
    ldr  r3, =g_sdk
    str  sp, [r3, #SDK_SCHEDULER_SP]
    str  lr, [r3, #SDK_SCHEDULER_LR]
    str  r0, [r3, #SDK_CURRENT_TASK]

    ldrb r1, [r0, #TASK_SUSPENDED]
    cmp  r1, #0
    bne  .Lresume_task

    @ Fresh entry: sp = stack_base + stack_size, blx entry.
    ldr  r1, [r0, #TASK_STACK_BASE]
    ldr  r2, [r0, #TASK_STACK_SIZE]
    add  r1, r1, r2
    mov  sp, r1

    ldr  r1, [r0, #TASK_ENTRY]
    blx  r1

.Lreturn_to_sched:
    ldr  r3, =g_sdk
    ldr  r1, [r3, #SDK_SCHEDULER_SP]
    ldr  lr, [r3, #SDK_SCHEDULER_LR]
    mov  sp, r1
    bx   lr

.Lresume_task:
    mov  r1, #0
    strb r1, [r0, #TASK_SUSPENDED]

    add  r1, r0, #TASK_SAVED_REGS
    ldr  r2, [r0, #TASK_SAVED_SP]
    ldr  r3, [r0, #TASK_RESUME_LR]
    ldm  r1, {r4-r11}
    mov  sp, r2
    bx   r3

    .ltorg
.size sdk_enter_or_resume, . - sdk_enter_or_resume


@ void fr_yield(void)
@
@ Suspend the current task; on the next tick sdk_enter_or_resume
@ restores it from where this returned.

.global fr_yield
.type   fr_yield, %function
.thumb_func
fr_yield:
    ldr  r3, =g_sdk
    ldr  r0, [r3, #SDK_CURRENT_TASK]

    add  r1, r0, #TASK_SAVED_REGS
    stm  r1, {r4-r11}
    mov  r2, sp
    str  r2, [r0, #TASK_SAVED_SP]
    str  lr, [r0, #TASK_RESUME_LR]

    mov  r1, #1
    strb r1, [r0, #TASK_SUSPENDED]

    ldr  r1, [r3, #SDK_SCHEDULER_SP]
    ldr  lr, [r3, #SDK_SCHEDULER_LR]
    mov  sp, r1
    bx   lr

    .ltorg
.size fr_yield, . - fr_yield
