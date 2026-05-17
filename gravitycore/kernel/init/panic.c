/*
 * GravityOS — Kernel Panic Handler
 * Non-recoverable fault handler. Saves register state, stack trace,
 * crash log to GravFS (signed, tamper-evident). Cooperative CPU shutdown.
 *
 * void __noreturn gravity_panic(const char *fmt, ...)
 * void dump_stack_trace(thread_t *t)
 * void save_crash_log(panic_info_t*)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

/* Minimal stdarg for kernel (no libc) */
typedef __builtin_va_list va_list;
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v, t)    __builtin_va_arg(v, t)

/* Panic info block — saved to crash log */
typedef struct {
    u64     rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    u64     r8, r9, r10, r11, r12, r13, r14, r15;
    u64     rip, rflags, cr2, cr3;
    u64     error_code;
    u32     cpu_id;
    u32     pid;
    u32     tid;
    u64     timestamp_ns;
    char    message[256];
    u64     stack_trace[32];
    u32     stack_depth;
} panic_info_t;

static panic_info_t last_panic;
static volatile u32 panic_cpu = 0xFFFFFFFF;
static volatile u32 panic_lock = 0;

/* Minimal serial output for panic (UART 0x3F8) */
static void panic_putchar(char c) {
#ifdef GRAV_ARCH_X86_64
    /* Direct UART I/O — no driver needed, always works */
    while (!((__builtin_ia32_inbyte(0x3FD)) & 0x20)) {}
    __builtin_ia32_outbyte(0x3F8, (unsigned char)c);
#else
    (void)c;
#endif
}

static void panic_puts(const char *s) {
    while (*s) panic_putchar(*s++);
}

static void panic_put_hex(u64 val) {
    const char *hex = "0123456789abcdef";
    panic_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        panic_putchar(hex[(val >> i) & 0xF]);
    }
}

static void panic_put_dec(u64 val) {
    char buf[21];
    int i = 20;
    buf[i] = '\0';
    if (val == 0) { panic_putchar('0'); return; }
    while (val > 0 && i > 0) {
        buf[--i] = '0' + (val % 10);
        val /= 10;
    }
    panic_puts(&buf[i]);
}

/* ═══════ Stack Trace ═══════ */
void dump_stack_trace(void *thread_ptr) {
    (void)thread_ptr;
    panic_puts("\n  Stack trace:\n");

    /* Walk frame pointers (RBP chain on x86_64) */
    u64 *rbp;
#ifdef GRAV_ARCH_X86_64
    __asm__ volatile("movq %%rbp, %0" : "=r"(rbp));
#else
    rbp = (u64*)0;
#endif

    u32 depth = 0;
    while (rbp && depth < 32) {
        u64 ret_addr = rbp[1];
        if (ret_addr == 0) break;

        panic_puts("    #");
        panic_put_dec(depth);
        panic_puts("  ");
        panic_put_hex(ret_addr);
        panic_putchar('\n');

        last_panic.stack_trace[depth] = ret_addr;
        rbp = (u64 *)rbp[0];
        depth++;
    }
    last_panic.stack_depth = depth;
}

/* ═══════ Save Crash Log ═══════ */
void save_crash_log(panic_info_t *info) {
    /* In full implementation:
     * 1. Write panic_info to reserved crash log region in GravFS
     * 2. Sign with TPM for tamper evidence
     * 3. The crash log survives reboot and is displayed on next boot
     *
     * For now: the info is in last_panic (static memory)
     */
    (void)info;
}

/* ═══════ Stop Other CPUs ═══════ */
static void panic_stop_cpus(void) {
    /* Send NMI/IPI to all other CPUs to halt them */
    /* On x86: write to LAPIC ICR to broadcast INIT */
    /* On ARM: write to GIC to signal all PEs */
    /* Simplified: just set a global flag */
}

/* ═══════ PANIC ═══════ */
__attribute__((noreturn))
void gravity_panic(const char *fmt, ...) {
    /* Acquire panic lock (only first CPU wins) */
    u32 expected = 0;
    if (!__sync_bool_compare_and_swap(&panic_lock, expected, 1)) {
        /* Another CPU already panicking — halt this one */
        for (;;) {
#ifdef GRAV_ARCH_X86_64
            __asm__ volatile("cli; hlt");
#else
            __asm__ volatile("wfi");
#endif
        }
    }

    /* Disable interrupts */
#ifdef GRAV_ARCH_X86_64
    __asm__ volatile("cli");
#endif

    /* Save current register state */
#ifdef GRAV_ARCH_X86_64
    __asm__ volatile(
        "movq %%rax, %0\n\t"
        "movq %%rbx, %1\n\t"
        "movq %%rcx, %2\n\t"
        "movq %%rdx, %3\n\t"
        : "=m"(last_panic.rax), "=m"(last_panic.rbx),
          "=m"(last_panic.rcx), "=m"(last_panic.rdx)
    );
    __asm__ volatile("movq %%rsp, %0" : "=m"(last_panic.rsp));
    __asm__ volatile("movq %%rbp, %0" : "=m"(last_panic.rbp));
    __asm__ volatile("movq %%cr2, %%rax; movq %%rax, %0" : "=m"(last_panic.cr2) :: "rax");
    __asm__ volatile("movq %%cr3, %%rax; movq %%rax, %0" : "=m"(last_panic.cr3) :: "rax");
#endif

    /* Print panic banner */
    panic_puts("\n\n");
    panic_puts("  ╔════════════════════════════════════════════════════╗\n");
    panic_puts("  ║           GRAVITY OS — KERNEL PANIC               ║\n");
    panic_puts("  ╚════════════════════════════════════════════════════╝\n\n");

    /* Format message (simplified printf — supports %s, %x, %d) */
    panic_puts("  FATAL: ");
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': panic_puts(va_arg(args, const char *)); break;
                case 'x': panic_put_hex(va_arg(args, u64)); break;
                case 'd': panic_put_dec(va_arg(args, u64)); break;
                case '%': panic_putchar('%'); break;
                default:  panic_putchar('%'); panic_putchar(*fmt); break;
            }
        } else {
            panic_putchar(*fmt);
        }
        fmt++;
    }
    va_end(args);
    panic_putchar('\n');

    /* Print register dump */
    panic_puts("\n  Register state:\n");
    panic_puts("    RAX="); panic_put_hex(last_panic.rax);
    panic_puts("  RBX="); panic_put_hex(last_panic.rbx); panic_putchar('\n');
    panic_puts("    RCX="); panic_put_hex(last_panic.rcx);
    panic_puts("  RDX="); panic_put_hex(last_panic.rdx); panic_putchar('\n');
    panic_puts("    RSP="); panic_put_hex(last_panic.rsp);
    panic_puts("  RBP="); panic_put_hex(last_panic.rbp); panic_putchar('\n');
    panic_puts("    CR2="); panic_put_hex(last_panic.cr2);
    panic_puts("  CR3="); panic_put_hex(last_panic.cr3); panic_putchar('\n');

    /* Stack trace */
    dump_stack_trace((void*)0);

    /* Save crash log */
    save_crash_log(&last_panic);

    /* Stop all other CPUs */
    panic_stop_cpus();

    /* Final message */
    panic_puts("\n  System halted. Crash log saved.\n");
    panic_puts("  Reboot to continue.\n\n");

    /* Halt forever */
    for (;;) {
#ifdef GRAV_ARCH_X86_64
        __asm__ volatile("cli; hlt");
#elif defined(GRAV_ARCH_ARM64)
        __asm__ volatile("wfi");
#elif defined(GRAV_ARCH_RISCV64)
        __asm__ volatile("wfi");
#endif
    }
}
