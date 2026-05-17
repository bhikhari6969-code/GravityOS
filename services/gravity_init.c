/*
 * GravityOS — GravityInit (PID 1)
 * Parallel dependency-graph service orchestrator.
 * Each service is a sandboxed process with explicit capability grants.
 * Not systemd. Purpose-built.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>
#include <gravity/ipc.h>

#define INIT_MAX_SERVICES  128

/* Service state */
typedef enum {
    SVC_STOPPED = 0,
    SVC_STARTING,
    SVC_RUNNING,
    SVC_STOPPING,
    SVC_FAILED,
    SVC_DISABLED,
} svc_state_t;

/* Service descriptor */
typedef struct {
    char            name[64];
    char            binary[128];        /* Path in initrd/GravFS */
    svc_state_t     state;
    grav_pid_t      pid;
    u64             capabilities;       /* Granted caps bitmask */
    u32             restart_policy;     /* 0=no, 1=always, 2=on-failure */
    u32             restart_count;
    u32             max_restarts;
    i32             exit_code;

    /* Dependency graph */
    u32             depends_on[8];      /* Indices of dependencies */
    u32             dep_count;
    u32             dep_satisfied;      /* Count of deps that are running */

    /* Resource limits */
    u64             mem_limit_bytes;
    u32             cpu_shares;
    u32             priority;
} svc_desc_t;

static svc_desc_t services[INIT_MAX_SERVICES];
static u32 service_count = 0;

/* ═══════ Register a service ═══════ */
static u32 svc_register(const char *name, const char *binary, u64 caps, u32 prio) {
    if (service_count >= INIT_MAX_SERVICES) return (u32)-1;
    svc_desc_t *s = &services[service_count];

    u32 i;
    for (i = 0; name[i] && i < 63; i++) s->name[i] = name[i];
    s->name[i] = '\0';
    for (i = 0; binary[i] && i < 127; i++) s->binary[i] = binary[i];
    s->binary[i] = '\0';

    s->state = SVC_STOPPED;
    s->pid = 0;
    s->capabilities = caps;
    s->restart_policy = 1;  /* Always restart by default */
    s->restart_count = 0;
    s->max_restarts = 5;
    s->dep_count = 0;
    s->dep_satisfied = 0;
    s->priority = prio;
    s->mem_limit_bytes = 256ULL * 1024 * 1024; /* 256MB default */
    s->cpu_shares = 1024;

    return service_count++;
}

/* ═══════ Add dependency ═══════ */
static void svc_add_dep(u32 service_idx, u32 dep_idx) {
    svc_desc_t *s = &services[service_idx];
    if (s->dep_count < 8) {
        s->depends_on[s->dep_count++] = dep_idx;
    }
}

/* ═══════ Check if service can start ═══════ */
static int svc_deps_met(u32 idx) {
    svc_desc_t *s = &services[idx];
    for (u32 i = 0; i < s->dep_count; i++) {
        if (services[s->depends_on[i]].state != SVC_RUNNING)
            return 0;
    }
    return 1;
}

/* ═══════ Start a service ═══════ */
static grav_err_t svc_start(u32 idx) {
    svc_desc_t *s = &services[idx];
    if (s->state == SVC_RUNNING || s->state == SVC_STARTING)
        return GE_OK;

    if (!svc_deps_met(idx))
        return GE_BUSY;

    s->state = SVC_STARTING;

    /* Create sandboxed process:
     * 1. Create new address space
     * 2. Load binary from GravFS
     * 3. Grant only declared capabilities
     * 4. Set resource limits (memory, CPU)
     * 5. Start execution
     *
     * grav_proc_create(s->name, &proc);
     * grav_proc_load(proc, s->binary);
     * for each cap in s->capabilities: grav_cap_grant(cap, proc->pid, ...);
     * grav_proc_set_mem_limit(proc, s->mem_limit_bytes);
     * grav_proc_start(proc);
     * s->pid = proc->pid;
     */

    s->state = SVC_RUNNING;
    return GE_OK;
}

/* ═══════ Parallel boot orchestration ═══════ */
static void init_boot_services(void) {
    /* Register all system services in dependency order */

    /* Layer 0: No dependencies (can start immediately) */
    u32 gravfs  = svc_register("gravfs",     "/sbin/gravfs-daemon",
        GC_READ | GC_WRITE | GC_DMA, 200);

    u32 gravsec = svc_register("gravsec",    "/sbin/gravsec",
        GC_READ | GC_IRQ, 240);

    /* Layer 1: Depends on gravfs */
    u32 gravnet = svc_register("gravnet",    "/sbin/gravnet",
        GC_NET | GC_READ | GC_WRITE, 180);
    svc_add_dep(gravnet, gravfs);

    u32 gravlog = svc_register("gravlog",    "/sbin/gravlog",
        GC_READ | GC_WRITE, 150);
    svc_add_dep(gravlog, gravfs);

    /* Layer 2: Depends on gravfs + gravnet */
    u32 gravdisplay = svc_register("gravdisplay", "/sbin/gravdisplay",
        GC_DISPLAY | GC_DMA | GC_READ, 220);
    svc_add_dep(gravdisplay, gravfs);

    u32 gravaudio = svc_register("gravaudio",  "/sbin/gravaudio",
        GC_AUDIO | GC_DMA | GC_READ, 160);
    svc_add_dep(gravaudio, gravfs);

    u32 gravinput = svc_register("gravinput",  "/sbin/gravinput",
        GC_READ | GC_IRQ, 170);
    svc_add_dep(gravinput, gravfs);

    u32 gravpkg = svc_register("gravpkg",    "/sbin/gravpkg",
        GC_READ | GC_WRITE | GC_NET, 100);
    svc_add_dep(gravpkg, gravfs);
    svc_add_dep(gravpkg, gravnet);

    /* Layer 3: Depends on display + net */
    u32 gravmind = svc_register("gravmind",   "/sbin/gravmind",
        GC_READ | GC_WRITE | GC_DISPLAY | GC_AUDIO | GC_NET, 230);
    svc_add_dep(gravmind, gravdisplay);
    svc_add_dep(gravmind, gravnet);

    /* Layer 4: GravityShell — depends on everything */
    u32 gravshell = svc_register("gravshell",  "/sbin/gravshell",
        GC_READ | GC_WRITE | GC_DISPLAY | GC_AUDIO | GC_NET, 250);
    svc_add_dep(gravshell, gravdisplay);
    svc_add_dep(gravshell, gravinput);
    svc_add_dep(gravshell, gravmind);

    /* ── Parallel boot: start all services whose deps are met ── */
    int progress = 1;
    while (progress) {
        progress = 0;
        for (u32 i = 0; i < service_count; i++) {
            if (services[i].state == SVC_STOPPED && svc_deps_met(i)) {
                svc_start(i);
                progress = 1;
            }
        }
    }
}

/* ═══════ Service supervisor (runs forever) ═══════ */
static void init_supervisor_loop(void) {
    for (;;) {
        /* Wait for child process exit notifications via IPC */
        /* gc_ipc_msg_t msg;
         * grav_ipc_recv(init_endpoint, &msg);
         *
         * If a service died:
         *   1. Check restart policy
         *   2. If restart_count < max_restarts: restart
         *   3. If critical service: panic
         *   4. Log to gravlog
         */

        /* Also handle: shutdown request, reboot, service control commands */
    }
}

/* ═══════ GravityInit Entry Point ═══════ */
void gravity_init_main(void) {
    /* We are PID 1. The kernel gave us full capabilities.
     * Our job: start everything, supervise everything. */

    /* Mount root filesystem */
    /* vfs_mount("/dev/grav0", "/", &gravfs_ops); */

    /* Start all services in dependency order */
    init_boot_services();

    /* Enter supervisor loop */
    init_supervisor_loop();
}
