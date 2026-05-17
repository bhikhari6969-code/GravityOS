/*
 * GravityOS — kernel_main()
 * gravitycore/kernel/init/main.c — Subsystem initialization order
 * This is where the kernel begins after arch-specific boot hands off.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

/* Forward declarations — each subsystem exposes a single init function */
extern grav_err_t grav_cap_init(void);
extern grav_err_t grav_pmm_init(grav_paddr_t mem_start, grav_size_t mem_size);
extern grav_err_t grav_vmm_init(void);
extern grav_err_t grav_slab_init(void);
extern grav_err_t grav_sched_init(void);
extern grav_err_t grav_ipc_init(void);
extern grav_err_t grav_irq_init(void);
extern grav_err_t grav_vfs_init(void);
extern grav_err_t gravio_init_subsystem(void);
extern void       grav_syscall_init(void);
extern void       grav_log(u32 level, const char *fmt, ...);
extern void       grav_panic(const char *fmt, ...) __attribute__((noreturn));

/* Boot info passed from arch-specific boot code */
typedef struct {
    grav_paddr_t  mem_start;
    grav_size_t   mem_size;
    grav_paddr_t  initrd_start;
    grav_size_t   initrd_size;
    grav_paddr_t  framebuffer;
    u32           fb_width;
    u32           fb_height;
    u32           fb_pitch;
    const char   *cmdline;
    u32           cpu_count;
    u32           arch_id;
} grav_bootinfo_t;

#define LOG_INFO  1
#define LOG_PANIC 4

/*
 * kernel_main — called once by each architecture's boot path.
 * Initializes every subsystem in dependency order, then launches
 * GravityInit (PID 1) as the first userspace process.
 */
void kernel_main(grav_bootinfo_t *boot) {
    grav_err_t err;

    grav_log(LOG_INFO, "");
    grav_log(LOG_INFO, "  ╔═══════════════════════════════════════╗");
    grav_log(LOG_INFO, "  ║         G R A V I T Y O S             ║");
    grav_log(LOG_INFO, "  ║     GravityCore v0.1.0 Singularity    ║");
    grav_log(LOG_INFO, "  ╚═══════════════════════════════════════╝");
    grav_log(LOG_INFO, "");
    grav_log(LOG_INFO, "kernel: booting on %u CPU(s), %llu MB RAM",
             boot->cpu_count, boot->mem_size / (1024 * 1024));
    if (boot->cmdline)
        grav_log(LOG_INFO, "kernel: cmdline = \"%s\"", boot->cmdline);

    /* ── Phase 1: Memory ── */
    grav_log(LOG_INFO, "kernel: [1/9] physical memory manager");
    err = grav_pmm_init(boot->mem_start, boot->mem_size);
    if (err != GE_OK) grav_panic("pmm_init failed: %d", err);

    grav_log(LOG_INFO, "kernel: [2/9] virtual memory manager");
    err = grav_vmm_init();
    if (err != GE_OK) grav_panic("vmm_init failed: %d", err);

    grav_log(LOG_INFO, "kernel: [3/9] SLAB allocator");
    err = grav_slab_init();
    if (err != GE_OK) grav_panic("slab_init failed: %d", err);

    /* ── Phase 2: Security ── */
    grav_log(LOG_INFO, "kernel: [4/9] capability system");
    err = grav_cap_init();
    if (err != GE_OK) grav_panic("cap_init failed: %d", err);

    /* ── Phase 3: Scheduling ── */
    grav_log(LOG_INFO, "kernel: [5/9] scheduler (hybrid CFS + RT + EDF)");
    err = grav_sched_init();
    if (err != GE_OK) grav_panic("sched_init failed: %d", err);

    /* ── Phase 4: Communication ── */
    grav_log(LOG_INFO, "kernel: [6/9] IPC subsystem");
    err = grav_ipc_init();
    if (err != GE_OK) grav_panic("ipc_init failed: %d", err);

    grav_log(LOG_INFO, "kernel: [7/9] IRQ subsystem");
    err = grav_irq_init();
    if (err != GE_OK) grav_panic("irq_init failed: %d", err);

    /* ── Phase 5: I/O & Filesystem ── */
    grav_log(LOG_INFO, "kernel: [8/9] VFS layer");
    err = grav_vfs_init();
    if (err != GE_OK) grav_panic("vfs_init failed: %d", err);

    grav_log(LOG_INFO, "kernel: [9/9] syscall table (64 handlers)");
    grav_syscall_init();

    /* ── Boot complete ── */
    grav_log(LOG_INFO, "");
    grav_log(LOG_INFO, "kernel: all subsystems initialized");
    grav_log(LOG_INFO, "kernel: launching GravityInit (PID 1)...");

    /*
     * Load and exec GravityInit from initrd.
     * GravityInit is a user-space process that:
     *   1. Mounts GravFS root filesystem
     *   2. Starts GravitySec (security daemon)
     *   3. Starts GravDisplay (compositor)
     *   4. Starts GravNet (network stack)
     *   5. Starts Gravity AI (GravMind)
     *   6. Launches GravityShell
     *
     * All services receive explicit capability grants — zero-trust.
     */

    /* grav_proc_exec_initrd(boot->initrd_start, boot->initrd_size,
     *                       "/sbin/gravity-init", ...);
     */

    grav_log(LOG_INFO, "kernel: entering idle loop");
    /* Main kernel loop — just handles interrupts and scheduling */
    for (;;) {
        /* grav_arch_halt(); — wait for next interrupt */
    }
}
