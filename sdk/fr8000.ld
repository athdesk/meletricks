/*
 * Base linker script for fr8000 user-image projects.
 *
 * Build with -ffunction-sections -fdata-sections --gc-sections.
 * Add --emit-relocs if you want to use relocating loader. (Good idea
 * for future proofing as we don't know if all firmwares will have the same
 * memory layout.)
 *
 * At least one FR_TASK is required; FR_SETUP is optional and limited
 * to one.  Both are enforced by ASSERTs below.
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(sdk_entry)

/* TODO: This name is not that good since we technically support
 * relocation (so we could be in SRAM or also FLASH). */

PROVIDE(PSRAM_ADDR = 0x220e0000);

SECTIONS
{
    /* sdk_entry pinned at the front for ease of loading */
    .entry PSRAM_ADDR :
    {
        KEEP(*(.entry))
        KEEP(*(.entry.*))
        . = ALIGN(4);
    }

    .text :
    {
        /* FR_SETUP body lives in its own section so the dispatcher
         * can reach it by linker symbol regardless of function name. */
        _fr_user_setup_start = .;
        KEEP(*(.fr_user_setup))
        _fr_user_setup_end = .;
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.rodata*)
        . = ALIGN(4);
    }

    /* FR_TASK emits one sdk_task_t struct per task into this section.
     * sdk_run_tick walks the [start, end) range. */
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

    /* NOLOAD keeps .bss as SHT_NOBITS; the installer zero-fills
     * [_sbss, _ebss) at upload time. */
    .bss (NOLOAD) : ALIGN(8)
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    }

    _end = _ebss;

    /* FR_SETUP marker: each FR_SETUP expansion drops one byte here.
     * NOLOAD keeps it out of the ELF; we only need the size for the
     * ASSERT. */
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

INCLUDE fr8000_syscalls.ld
