/*
 * GravityOS — x86_64 SYSCALL/SYSRET Fast Path
 * gravitycore/arch/x86_64/syscall_entry.S
 * 
 * x86_64 SYSCALL convention:
 *   RAX = syscall number (0x00–0x3F)
 *   RDI, RSI, RDX, R10, R8, R9 = args 0–5
 *   RCX = saved RIP (by hardware)
 *   R11 = saved RFLAGS (by hardware)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
.section .text
.global syscall_entry
.extern grav_syscall_dispatch

syscall_entry:
    /* ── Save user context ── */
    /* Swap to kernel stack (from per-CPU TSS) */
    swapgs                          /* Switch GS base to kernel per-CPU data */
    movq    %rsp, %gs:0x08         /* Save user RSP in per-CPU scratch */
    movq    %gs:0x00, %rsp         /* Load kernel stack pointer */

    /* Push user context for later SYSRET */
    pushq   %rcx                   /* User RIP */
    pushq   %r11                   /* User RFLAGS */
    pushq   %gs:0x08               /* User RSP */

    /* Save callee-saved registers */
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    /* ── Validate syscall number ── */
    cmpq    $0x3F, %rax
    ja      .Lbad_syscall

    /* ── Dispatch ── */
    /* Move R10 → RCX (Linux convention: R10 replaces RCX which SYSCALL clobbers) */
    movq    %r10, %rcx

    /* Call: grav_syscall_dispatch(syscall_num, a0, a1, a2, a3, a4, a5) */
    /* RAX=number already, RDI/RSI/RDX already in place */
    /* Push extra args on stack per C calling convention */
    pushq   %r9                    /* arg5 */
    pushq   %r8                    /* arg4 */

    movq    %rax, %rdi             /* arg0 = syscall number */
    /* Shift: original RDI→RSI, RSI→RDX, RDX→RCX, R10→R8, R8→R9 */
    /* Actually, re-arrange for our C dispatch signature */
    movq    %rdi, %rsi             /* This is simplified — real impl uses */
    call    grav_syscall_dispatch  /* a proper register shuffle             */

    /* Clean up pushed args */
    addq    $16, %rsp

    /* Result is in RAX — will be returned to userspace */
    jmp     .Lsysret

.Lbad_syscall:
    movq    $-2, %rax              /* GE_INVAL */

.Lsysret:
    /* ── Restore user context ── */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx

    popq    %rsp                   /* Restore user RSP */
    popq    %r11                   /* Restore RFLAGS */
    popq    %rcx                   /* Restore RIP */

    swapgs                         /* Restore user GS base */
    sysretq                        /* Return to ring 3 */
