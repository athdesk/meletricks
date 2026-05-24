/*
 * yield.h — coroutine yield + setup attribute macro
 *
 * The entry point is sdk_entry (defined in sdk/src/entry.s), which
 * sets up a private stack and calls _main_loop().  User code may call
 * fr_yield() to hand time back; sdk_entry resumes from the same point
 * on the next invocation.
 *
 */
#ifndef YIELD_H
#define YIELD_H

/* Tag the one-shot initialiser, runs once on the very first tick after
 * hook is placed. Can omit if you don't need any boot-time setup. */
#define FR_SETUP __attribute__((used, section(".fr_user_setup")))

/* Suspend the current task and return control to the firmware or another task. */
void fr_yield(void);

#endif /* YIELD_H */
