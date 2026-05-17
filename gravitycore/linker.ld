/*
 * GravityOS — Kernel Linker Script
 * Defines section layout for the GravityCore kernel image.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;  /* Kernel virtual base (upper -2GB) */
KERNEL_LMA = 0x0000000000100000;  /* Physical load address (1MB) */

SECTIONS
{
    . = KERNEL_VMA;

    /* ── Code ── */
    .text ALIGN(4K) : AT(KERNEL_LMA)
    {
        _text_start = .;
        *(.text.boot)           /* Boot entry must come first */
        *(.text .text.*)
        _text_end = .;
    }

    /* ── Read-Only Data ── */
    .rodata ALIGN(4K) : AT(KERNEL_LMA + (_rodata_start - _text_start))
    {
        _rodata_start = .;
        *(.rodata .rodata.*)
        _rodata_end = .;
    }

    /* ── Initialized Data ── */
    .data ALIGN(4K) : AT(KERNEL_LMA + (_data_start - _text_start))
    {
        _data_start = .;
        *(.data .data.*)
        _data_end = .;
    }

    /* ── BSS (Zero-Initialized) ── */
    .bss ALIGN(4K) : AT(KERNEL_LMA + (_bss_start - _text_start))
    {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;
    _kernel_size = _kernel_end - KERNEL_VMA;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}
