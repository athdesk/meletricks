/*
 * emu.ld — guest linker script for the host emulator build.
 *
 * Differs from the on-device fr8000.ld in three ways:
 *   1. No ROM at 0x0; the SDK isn't compiled in either.
 *   2. Each SDK API function is PROVIDE()d at a fixed trap address
 *      in 0x60000000..; the host catches execution with UC_HOOK_CODE
 *      and runs a Python handler.
 *   3. A stack region is reserved in PSRAM and __stack_top exported so
 *      the host can initialize SP before calling into the guest.
 *
 * Layout mirrors the device for everything the app cares about:
 *   .text/.rodata/.data/.bss live in PSRAM starting at 0x220e0000,
 *   the framebuffer window at 0x22000000 is left to the host to map.
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)

PSRAM_ADDR = 0x220e0000;
STACK_SIZE = 0x10000;            /* 64 KB host-side scheduler+task stack */

/* ── SDK trap region ─────────────────────────────────────────────────
 * Each function gets a unique 4-byte slot; low bit set marks Thumb.
 * Address layout is hand-assigned (stable, so logs stay readable).
 */
PROVIDE(fr_micros              = 0x60000001);
PROVIDE(fr_millis              = 0x60000005);

PROVIDE(srand                  = 0x60000009);
PROVIDE(rand                   = 0x6000000D);

PROVIDE(ke_malloc              = 0x60000011);
PROVIDE(ke_free                = 0x60000015);

PROVIDE(kbd_event_pop          = 0x60000019);
PROVIDE(kbd_battery_get        = 0x6000001D);
PROVIDE(kbd_battery_set_callback = 0x60000021);
PROVIDE(kbd_status_get         = 0x60000025);
PROVIDE(kbd_status_set_callback = 0x60000029);

PROVIDE(gui_pause              = 0x6000002D);
PROVIDE(lcd_te_sync_disable    = 0x60000031);
PROVIDE(lcd_is_idle            = 0x60000035);
PROVIDE(lcd_sleep              = 0x60000039);
PROVIDE(lcd_wake               = 0x6000003D);

PROVIDE(m_draw_frame           = 0x60000041);

PROVIDE(sdk_get_fw_override    = 0x60000045);
PROVIDE(sdk_set_fw_override    = 0x60000049);

ENTRY(_emu_entry)

SECTIONS
{
    . = PSRAM_ADDR;

    .text : ALIGN(4)
    {
        _stext = .;
        /* Synthetic entry sentinel; host never executes it but ENTRY
         * needs a target.  Provided by guest_stubs.c. */
        KEEP(*(.emu_entry))

        /* FR_SETUP body — host runs this once on the first tick. */
        _fr_user_setup_start = .;
        KEEP(*(.fr_user_setup))
        _fr_user_setup_end = .;

        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    }

    /* FR_TASK emits one sdk_task_t per task here; host walks the
     * [start, end) range each tick. */
    .fr_user_tasks_table : ALIGN(4)
    {
        _fr_user_tasks_table_start = .;
        KEEP(*(.fr_user_tasks_table))
        _fr_user_tasks_table_end = .;
    }
    ASSERT(_fr_user_tasks_table_end > _fr_user_tasks_table_start,
           "No FR_TASK defined — every user image needs at least one.")

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    }

    .bss (NOLOAD) : ALIGN(8)
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    }

    /* Heap for the bump allocator backing ke_malloc.  Sits between
     * .bss and the stack; size is whatever's left up to STACK. */
    . = ALIGN(16);
    _heap_start = .;
    . = . + 0x40000;                 /* 256 KB heap; bump-allocated  */
    _heap_end   = .;

    . = ALIGN(16);
    _stack_bottom = .;
    . = . + STACK_SIZE;
    _stack_top    = .;

    _end = .;

    /* FR_SETUP marker — same enforcement as on-device build. */
    .fr_user_setup_marker (NOLOAD) : { KEEP(*(.fr_user_setup_marker)) }
    ASSERT(SIZEOF(.fr_user_setup_marker) <= 1,
           "More than one FR_SETUP function defined — only one is allowed per program.")

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.note*)
        *(.comment*)
    }
}
