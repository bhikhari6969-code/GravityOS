/*
 * GravityOS — Scheduler
 * Main scheduler. Per-CPU run queues. CFS for normal, RT for real-time,
 * AI-domain for GravMind threads. Called on every timer tick + yield.
 *
 * void       schedule(void)
 * thread_t  *pick_next_thread(cpu_t *cpu)
 * void       thread_yield(void)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define SCHED_MAX_CPUS     256
#define SCHED_MAX_THREADS  4096
#define SCHED_CFS_QUANTUM  10000000ULL   /* 10ms in nanoseconds */
#define SCHED_RT_QUANTUM   1000000ULL    /* 1ms for RT */

/* Scheduling classes */
typedef enum {
    SCHED_CLASS_IDLE   = 0,
    SCHED_CLASS_NORMAL = 1,   /* CFS */
    SCHED_CLASS_BATCH  = 2,   /* Low-priority background */
    SCHED_CLASS_RT     = 3,   /* Real-time FIFO/RR */
    SCHED_CLASS_AI     = 4,   /* GravMind AI inference */
    SCHED_CLASS_DL     = 5,   /* Deadline (EDF) */
} sched_class_t;

/* Thread state */
typedef enum {
    THREAD_RUNNING    = 0,
    THREAD_READY      = 1,
    THREAD_BLOCKED    = 2,
    THREAD_SLEEPING   = 3,
    THREAD_DEAD       = 4,
} thread_state_t;

/* Thread Control Block */
typedef struct thread {
    grav_tid_t      tid;
    grav_pid_t      pid;
    thread_state_t  state;
    sched_class_t   sclass;

    /* CFS fields */
    u64             vruntime;         /* Virtual runtime (weighted) */
    u64             sum_exec;         /* Total CPU time consumed */
    u32             nice;             /* -20 to +19, maps to weight */
    u32             weight;           /* CFS weight from nice value */

    /* RT fields */
    u32             rt_priority;      /* 0–99 (99 = highest) */
    u32             rt_policy;        /* 0=FIFO, 1=RR */
    u64             rt_time_slice;    /* Remaining time for RR */

    /* Deadline fields */
    u64             dl_deadline_ns;
    u64             dl_runtime_ns;
    u64             dl_period_ns;

    /* Context */
    grav_vaddr_t    kernel_stack;
    grav_vaddr_t    user_stack;
    void           *context;          /* Saved registers (arch-specific) */
    u32             cpu_affinity;     /* Preferred CPU (-1 = any) */
    u32             on_cpu;           /* Currently scheduled on this CPU */

    /* Capability */
    grav_cap_t      cap;

    /* Stats */
    u64             start_time_ns;
    u64             last_scheduled_ns;
    u64             voluntary_switches;
    u64             involuntary_switches;

    /* Linked list for run queue */
    struct thread  *rq_next;
    struct thread  *rq_prev;
} thread_t;

/* Per-CPU state */
typedef struct cpu {
    u32         cpu_id;
    thread_t   *current;          /* Currently running thread */
    thread_t   *idle_thread;

    /* CFS run queue (sorted by vruntime — red-black tree simplified as list) */
    thread_t   *cfs_head;
    u32         cfs_count;
    u64         cfs_min_vruntime;

    /* RT run queue (priority-ordered) */
    thread_t   *rt_head;
    u32         rt_count;

    /* Deadline run queue */
    thread_t   *dl_head;
    u32         dl_count;

    /* Load tracking */
    u64         total_load;
    u64         avg_load;
    u64         tick_count;

    /* Need-resched flag */
    u32         need_resched;
    u32         preempt_count;    /* >0 = preemption disabled */
} cpu_t;

static cpu_t cpus[SCHED_MAX_CPUS];
static u32 cpu_count = 0;
static thread_t threads[SCHED_MAX_THREADS];
static u32 thread_count = 0;

/* Nice-to-weight mapping (Linux-compatible) */
static const u32 nice_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */  9548,  7620,  6100,  4904,  3906,
    /*  -5 */  3121,  2501,  1991,  1586,  1277,
    /*   0 */  1024,   820,   655,   526,   423,
    /*   5 */   335,   272,   215,   172,   137,
    /*  10 */   110,    87,    70,    56,    45,
    /*  15 */    36,    29,    23,    18,    15,
};

/* ═══════ Init ═══════ */
grav_err_t grav_sched_init(void) {
    for (u32 i = 0; i < SCHED_MAX_CPUS; i++) {
        cpus[i].cpu_id = i;
        cpus[i].current = (thread_t*)0;
        cpus[i].idle_thread = (thread_t*)0;
        cpus[i].cfs_head = (thread_t*)0;
        cpus[i].cfs_count = 0;
        cpus[i].cfs_min_vruntime = 0;
        cpus[i].rt_head = (thread_t*)0;
        cpus[i].rt_count = 0;
        cpus[i].dl_head = (thread_t*)0;
        cpus[i].dl_count = 0;
        cpus[i].total_load = 0;
        cpus[i].avg_load = 0;
        cpus[i].tick_count = 0;
        cpus[i].need_resched = 0;
        cpus[i].preempt_count = 0;
    }
    thread_count = 0;
    return GE_OK;
}

/* ═══════ Create Thread ═══════ */
thread_t *thread_create(grav_pid_t pid, sched_class_t sclass,
                         i32 nice_val, grav_vaddr_t entry) {
    if (thread_count >= SCHED_MAX_THREADS) return (thread_t*)0;

    thread_t *t = &threads[thread_count];
    t->tid = thread_count++;
    t->pid = pid;
    t->state = THREAD_READY;
    t->sclass = sclass;
    t->vruntime = 0;
    t->sum_exec = 0;
    t->nice = (u32)(nice_val + 20); /* Shift to 0–39 range */
    t->weight = nice_to_weight[t->nice < 40 ? t->nice : 20];
    t->rt_priority = 0;
    t->rt_policy = 0;
    t->cpu_affinity = (u32)-1; /* Any CPU */
    t->voluntary_switches = 0;
    t->involuntary_switches = 0;
    t->rq_next = (thread_t*)0;
    t->rq_prev = (thread_t*)0;

    (void)entry;
    return t;
}

/* ═══════ CFS: Insert into run queue (sorted by vruntime) ═══════ */
static void cfs_enqueue(cpu_t *cpu, thread_t *t) {
    /* Update vruntime: new threads start at min_vruntime to prevent starvation */
    if (t->vruntime < cpu->cfs_min_vruntime)
        t->vruntime = cpu->cfs_min_vruntime;

    /* Insert sorted by vruntime (leftmost = smallest = runs next) */
    if (!cpu->cfs_head || t->vruntime <= cpu->cfs_head->vruntime) {
        t->rq_next = cpu->cfs_head;
        cpu->cfs_head = t;
    } else {
        thread_t *cur = cpu->cfs_head;
        while (cur->rq_next && cur->rq_next->vruntime < t->vruntime)
            cur = cur->rq_next;
        t->rq_next = cur->rq_next;
        cur->rq_next = t;
    }
    cpu->cfs_count++;
}

/* ═══════ CFS: Dequeue head ═══════ */
static thread_t *cfs_dequeue(cpu_t *cpu) {
    if (!cpu->cfs_head) return (thread_t*)0;
    thread_t *t = cpu->cfs_head;
    cpu->cfs_head = t->rq_next;
    t->rq_next = (thread_t*)0;
    cpu->cfs_count--;
    return t;
}

/* ═══════ RT: Insert by priority (highest first) ═══════ */
static void rt_enqueue(cpu_t *cpu, thread_t *t) {
    if (!cpu->rt_head || t->rt_priority > cpu->rt_head->rt_priority) {
        t->rq_next = cpu->rt_head;
        cpu->rt_head = t;
    } else {
        thread_t *cur = cpu->rt_head;
        while (cur->rq_next && cur->rq_next->rt_priority >= t->rt_priority)
            cur = cur->rq_next;
        t->rq_next = cur->rq_next;
        cur->rq_next = t;
    }
    cpu->rt_count++;
}

/* ═══════ Pick Next Thread ═══════ */
thread_t *pick_next_thread(cpu_t *cpu) {
    /* Priority: DL > RT > AI > CFS > IDLE */

    /* 1. Deadline (EDF — earliest deadline first) */
    if (cpu->dl_head) {
        thread_t *t = cpu->dl_head;
        cpu->dl_head = t->rq_next;
        t->rq_next = (thread_t*)0;
        cpu->dl_count--;
        return t;
    }

    /* 2. Real-time (highest static priority) */
    if (cpu->rt_head) {
        thread_t *t = cpu->rt_head;
        cpu->rt_head = t->rq_next;
        t->rq_next = (thread_t*)0;
        cpu->rt_count--;
        return t;
    }

    /* 3. CFS (smallest vruntime) */
    if (cpu->cfs_head) {
        return cfs_dequeue(cpu);
    }

    /* 4. Idle thread */
    return cpu->idle_thread;
}

/* ═══════ schedule() — main scheduler entry ═══════ */
void schedule(void) {
    /* Get current CPU */
    u32 cpu_id = 0; /* arch_get_cpu_id(); */
    cpu_t *cpu = &cpus[cpu_id];

    cpu->need_resched = 0;

    thread_t *prev = cpu->current;
    thread_t *next = pick_next_thread(cpu);

    if (next == prev) return; /* Same thread, no switch needed */

    if (prev && prev->state == THREAD_RUNNING) {
        /* Re-enqueue previous thread */
        prev->state = THREAD_READY;
        if (prev->sclass == SCHED_CLASS_RT || prev->sclass == SCHED_CLASS_AI) {
            rt_enqueue(cpu, prev);
            prev->involuntary_switches++;
        } else if (prev->sclass == SCHED_CLASS_NORMAL ||
                   prev->sclass == SCHED_CLASS_BATCH) {
            /* Update vruntime: delta = (actual_runtime * 1024) / weight */
            u64 now = 0; /* arch_read_tsc(); */
            u64 delta = now - prev->last_scheduled_ns;
            prev->sum_exec += delta;
            prev->vruntime += (delta * 1024) / prev->weight;
            cfs_enqueue(cpu, prev);
            prev->involuntary_switches++;
        }
    }

    /* Switch to next */
    if (next) {
        next->state = THREAD_RUNNING;
        next->on_cpu = cpu_id;
        next->last_scheduled_ns = 0; /* arch_read_tsc(); */
        cpu->current = next;
        cpu->tick_count++;

        /* Update min_vruntime */
        if (cpu->cfs_head)
            cpu->cfs_min_vruntime = cpu->cfs_head->vruntime;

        /* Perform context switch (arch-specific) */
        /* arch_context_switch(prev->context, next->context); */
    }
}

/* ═══════ thread_yield() — voluntary preemption ═══════ */
void thread_yield(void) {
    u32 cpu_id = 0; /* arch_get_cpu_id(); */
    cpu_t *cpu = &cpus[cpu_id];

    thread_t *cur = cpu->current;
    if (cur) {
        cur->voluntary_switches++;
        cur->state = THREAD_READY;
    }

    schedule();
}

/* ═══════ Timer Tick Handler ═══════ */
void sched_timer_tick(void) {
    u32 cpu_id = 0;
    cpu_t *cpu = &cpus[cpu_id];
    thread_t *cur = cpu->current;
    if (!cur) return;

    /* Check if time slice expired */
    u64 now = 0; /* arch_read_tsc(); */
    u64 runtime = now - cur->last_scheduled_ns;

    if (cur->sclass == SCHED_CLASS_NORMAL && runtime >= SCHED_CFS_QUANTUM) {
        cpu->need_resched = 1;
    } else if (cur->sclass == SCHED_CLASS_RT && cur->rt_policy == 1 /* RR */) {
        if (runtime >= SCHED_RT_QUANTUM) {
            cpu->need_resched = 1;
        }
    }

    /* Check deadline threads */
    /* If a deadline thread's deadline is approaching, preempt */

    if (cpu->need_resched) schedule();
}

/* ═══════ Enqueue thread on CPU ═══════ */
void sched_enqueue(thread_t *t) {
    /* Pick CPU: use affinity or lowest-load CPU */
    u32 target_cpu = 0;
    if (t->cpu_affinity != (u32)-1 && t->cpu_affinity < cpu_count) {
        target_cpu = t->cpu_affinity;
    } else {
        /* Find CPU with lowest load */
        u64 min_load = cpus[0].total_load;
        for (u32 i = 1; i < cpu_count; i++) {
            if (cpus[i].total_load < min_load) {
                min_load = cpus[i].total_load;
                target_cpu = i;
            }
        }
    }

    cpu_t *cpu = &cpus[target_cpu];
    t->state = THREAD_READY;

    switch (t->sclass) {
    case SCHED_CLASS_RT:
    case SCHED_CLASS_AI:
        rt_enqueue(cpu, t);
        break;
    case SCHED_CLASS_NORMAL:
    case SCHED_CLASS_BATCH:
    default:
        cfs_enqueue(cpu, t);
        break;
    }

    cpu->total_load += t->weight;
}
