/*
 * GravityOS — x86_64 Boot Entry
 * UEFI entry, long-mode setup, GDT/IDT init, then jump to kernel_main
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
.section .text.boot
.global _start
.extern kernel_main

/* ═══════ UEFI Entry Point ═══════ */
/* UEFI hands us control in long mode with identity-mapped memory.
 * We set up our own GDT, IDT, page tables, then call kernel_main(). */
_start:
    /* Disable interrupts during boot setup */
    cli

    /* Save UEFI boot info pointer (passed in %rdi by convention) */
    movq    %rdi, %r15

    /* Set up our kernel stack */
    leaq    _kernel_stack_top(%rip), %rsp

    /* ── Load GDT ── */
    leaq    gdt_ptr(%rip), %rax
    lgdt    (%rax)

    /* Reload segment registers with kernel selectors */
    movw    $0x10, %ax          /* Kernel data segment = GDT entry 2 */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x00, %ax
    movw    %ax, %fs
    movw    %ax, %gs

    /* Far jump to reload CS with kernel code segment */
    pushq   $0x08               /* Kernel code segment = GDT entry 1 */
    leaq    .Lreload_cs(%rip), %rax
    pushq   %rax
    lretq

.Lreload_cs:
    /* ── Load IDT ── */
    leaq    idt_ptr(%rip), %rax
    lidt    (%rax)

    /* ── Enable SSE/AVX (required for modern code generation) ── */
    movq    %cr0, %rax
    andw    $0xFFFB, %ax        /* Clear CR0.EM (bit 2) */
    orw     $0x0002, %ax        /* Set CR0.MP (bit 1) */
    movq    %rax, %cr0
    movq    %cr4, %rax
    orw     $0x0600, %ax        /* Set CR4.OSFXSR + CR4.OSXMMEXCPT */
    movq    %rax, %cr4

    /* ── Enable SYSCALL/SYSRET ── */
    movl    $0xC0000080, %ecx   /* IA32_EFER MSR */
    rdmsr
    orl     $0x0001, %eax       /* Set SCE (Syscall Enable) bit */
    wrmsr

    /* Set LSTAR (syscall entry point) */
    movl    $0xC0000082, %ecx   /* IA32_LSTAR */
    leaq    syscall_entry(%rip), %rax
    movl    %eax, %eax          /* Low 32 bits → EAX (zero-extend) */
    movq    %rax, %rdx
    shrq    $32, %rdx           /* High 32 bits → EDX */
    wrmsr

    /* Set STAR (segment selectors for SYSCALL/SYSRET) */
    movl    $0xC0000081, %ecx   /* IA32_STAR */
    xorl    %eax, %eax
    movl    $0x00180008, %edx   /* SYSRET CS/SS=0x18|0x08, SYSCALL CS/SS=0x08|0x10 */
    wrmsr

    /* Set SFMASK (RFLAGS mask on SYSCALL — clear IF, TF, DF) */
    movl    $0xC0000084, %ecx   /* IA32_FMASK */
    movl    $0x00000700, %eax   /* Mask IF+TF+DF */
    xorl    %edx, %edx
    wrmsr

    /* ── Call kernel_main(boot_info) ── */
    movq    %r15, %rdi          /* Restore boot info pointer as arg0 */
    call    kernel_main

    /* kernel_main should never return */
    cli
.Lhalt:
    hlt
    jmp     .Lhalt

/* ═══════ GDT ═══════ */
.section .data
.align 16
gdt_start:
    .quad   0x0000000000000000  /* 0x00: Null descriptor */
    .quad   0x00AF9A000000FFFF  /* 0x08: Kernel Code (64-bit, ring 0) */
    .quad   0x00CF92000000FFFF  /* 0x10: Kernel Data (ring 0) */
    .quad   0x00AFFA000000FFFF  /* 0x18: User Code (64-bit, ring 3) */
    .quad   0x00CFF2000000FFFF  /* 0x20: User Data (ring 3) */
    /* TSS descriptor goes here (16 bytes, loaded dynamically) */
gdt_end:

gdt_ptr:
    .word   gdt_end - gdt_start - 1
    .quad   gdt_start

/* ═══════ IDT (filled at runtime by idt.c) ═══════ */
.align 16
idt_table:
    .fill   256, 16, 0          /* 256 entries × 16 bytes each */
idt_ptr:
    .word   (256 * 16) - 1
    .quad   idt_table

/* ═══════ Kernel Stack (16KB) ═══════ */
.section .bss
.align 4096
_kernel_stack_bottom:
    .skip   16384
_kernel_stack_top:
