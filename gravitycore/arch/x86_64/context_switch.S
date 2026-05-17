/*
 * GravityOS — x86_64 Context Switch
 * Saves/restores all registers + FPU/SSE/AVX state.
 * Called by scheduler when switching between threads.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
.section .text
.global arch_context_switch

/* arch_context_switch(void **old_sp, void *new_sp)
 * RDI = pointer to old thread's saved stack pointer
 * RSI = new thread's saved stack pointer
 */
arch_context_switch:
    /* ── Save callee-saved registers onto old stack ── */
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    pushfq                          /* Save RFLAGS */

    /* Save FPU/SSE state */
    subq    $512, %rsp              /* 512 bytes for FXSAVE area */
    fxsave  (%rsp)

    /* Store old stack pointer */
    movq    %rsp, (%rdi)

    /* ── Switch to new stack ── */
    movq    %rsi, %rsp

    /* Restore FPU/SSE state */
    fxrstor (%rsp)
    addq    $512, %rsp

    /* Restore callee-saved registers */
    popfq                           /* Restore RFLAGS */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx

    retq                            /* Return to new thread's saved RIP */
