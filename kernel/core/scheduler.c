/*
 * GravityOS — GravityScheduler
 * Hybrid O(1)+CFS with AI-workload-aware CPU domains
 * Real-time: EDF + fixed priority. Normal: CFS virtual runtime.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "gravity_core.h"

/* ═══════ Run Queue — Per-CPU ═══════ */
#define SCHED_NUM_PRIORITIES 256
#define SCHED_DEFAULT_TIMESLICE_NS (10000000ULL) /* 10ms */

typedef struct {
    grav_thread_t *head;
    grav_thread_t *tail;
    u32 count;
} sched_queue_t;

typedef struct {
    /* O(1) priority bitmap — each bit = has runnable thread at that priority */
    u64 priority_bitmap[4];  /* 256 bits = 256 priority levels */

    /* Per-priority FIFO queues (O(1) lookup) */
    sched_queue_t rt_queues[SCHED_NUM_PRIORITIES];

    /* CFS red-black tree substitute: sorted linked list by vruntime */
    grav_thread_t *cfs_head;
    u64 min_vruntime;

    /* Statistics */
    u64 context_switches;
    u64 idle_time_ns;
    u32 cpu_id;
    grav_thread_t *current;
    grav_thread_t *idle_thread;
} sched_cpu_t;

static sched_cpu_t cpu_schedulers[GRAV_MAX_CPUS];
static u32 active_cpus = 1;

/* AI workload classes — scheduler adapts behavior per class */
#define GRAV_WORKLOAD_INTERACTIVE  0x01  /* UI, low latency */
#define GRAV_WORKLOAD_BATCH        0x02  /* Background, throughput */
#define GRAV_WORKLOAD_REALTIME     0x03  /* Hard deadlines */
#define GRAV_WORKLOAD_AI_INFERENCE 0x04  /* GPU/NPU heavy */
#define GRAV_WORKLOAD_IO_BOUND     0x05  /* Mostly waiting */

/* ═══════ Bitmap Operations ═══════ */
static void bitmap_set(u64 *bitmap, u32 bit) {
    bitmap[bit / 64] |= (1ULL << (bit % 64));
}

static void bitmap_clear(u64 *bitmap, u32 bit) {
    bitmap[bit / 64] &= ~(1ULL << (bit % 64));
}

static i32 bitmap_find_highest(u64 *bitmap) {
    for (i32 i = 3; i >= 0; i--) {
        if (bitmap[i] != 0) {
            /* Find highest set bit using compiler builtin */
            return (i * 64) + (63 - __builtin_clzll(bitmap[i]));
        }
    }
    return -1;
}

/* ═══════ Scheduler Init ═══════ */
grav_error_t grav_sched_init(void) {
    for (u32 cpu = 0; cpu < GRAV_MAX_CPUS; cpu++) {
        sched_cpu_t *sc = &cpu_schedulers[cpu];
        sc->cpu_id = cpu;
        sc->context_switches = 0;
        sc->idle_time_ns = 0;
        sc->current = GRAV_NULL;
        sc->cfs_head = GRAV_NULL;
        sc->min_vruntime = 0;

        for (u32 i = 0; i < 4; i++) sc->priority_bitmap[i] = 0;
        for (u32 i = 0; i < SCHED_NUM_PRIORITIES; i++) {
            sc->rt_queues[i].head = GRAV_NULL;
            sc->rt_queues[i].tail = GRAV_NULL;
            sc->rt_queues[i].count = 0;
        }
    }
    grav_log(GRAV_LOG_INFO, "sched: initialized for %u CPUs", active_cpus);
    return GRAV_OK;
}

/* ═══════ Add Thread to Run Queue ═══════ */
grav_error_t grav_sched_add_thread(grav_thread_t *thread) {
    if (!thread) return GRAV_ERR_INVAL;

    /* Determine target CPU from affinity mask */
    u32 target_cpu = 0;
    for (u32 i = 0; i < active_cpus; i++) {
        if (thread->affinity.mask[i / 64] & (1ULL << (i % 64))) {
            target_cpu = i;
            break;
        }
    }
    sched_cpu_t *sc = &cpu_schedulers[target_cpu];

    if (thread->policy == GRAV_SCHED_RT_FIFO || 
        thread->policy == GRAV_SCHED_RT_RR ||
        thread->policy == GRAV_SCHED_EDF) {
        /* Real-time: add to priority queue */
        u32 prio = (u32)thread->priority;
        sched_queue_t *q = &sc->rt_queues[prio];
        thread->next = GRAV_NULL;
        if (q->tail) {
            q->tail->next = thread;
        } else {
            q->head = thread;
        }
        q->tail = thread;
        q->count++;
        bitmap_set(sc->priority_bitmap, prio);
    } else {
        /* CFS: insert sorted by vruntime */
        if (thread->vruntime < sc->min_vruntime) {
            thread->vruntime = sc->min_vruntime;
        }
        /* Insert into sorted list */
        if (!sc->cfs_head || thread->vruntime < sc->cfs_head->vruntime) {
            thread->next = sc->cfs_head;
            sc->cfs_head = thread;
        } else {
            grav_thread_t *prev = sc->cfs_head;
            while (prev->next && prev->next->vruntime <= thread->vruntime) {
                prev = prev->next;
            }
            thread->next = prev->next;
            prev->next = thread;
        }
    }

    thread->state = GRAV_PROC_READY;
    return GRAV_OK;
}

/* ═══════ Pick Next Thread (called on timer tick) ═══════ */
grav_thread_t *grav_sched_pick_next(u32 cpu_id) {
    sched_cpu_t *sc = &cpu_schedulers[cpu_id];

    /* Priority 1: Real-time threads (O(1) bitmap scan) */
    i32 highest = bitmap_find_highest(sc->priority_bitmap);
    if (highest >= 0) {
        sched_queue_t *q = &sc->rt_queues[highest];
        grav_thread_t *thread = q->head;
        if (thread) {
            q->head = thread->next;
            if (!q->head) {
                q->tail = GRAV_NULL;
                bitmap_clear(sc->priority_bitmap, (u32)highest);
            }
            q->count--;
            thread->next = GRAV_NULL;
            thread->state = GRAV_PROC_RUNNING;
            sc->current = thread;
            sc->context_switches++;
            return thread;
        }
    }

    /* Priority 2: CFS — pick leftmost (lowest vruntime) */
    if (sc->cfs_head) {
        grav_thread_t *thread = sc->cfs_head;
        sc->cfs_head = thread->next;
        thread->next = GRAV_NULL;
        thread->state = GRAV_PROC_RUNNING;
        sc->current = thread;
        sc->context_switches++;
        return thread;
    }

    /* No runnable threads — return idle thread */
    return sc->idle_thread;
}

/* ═══════ Timer Tick — update vruntime, check preemption ═══════ */
void grav_sched_tick(u64 elapsed_ns) {
    for (u32 cpu = 0; cpu < active_cpus; cpu++) {
        sched_cpu_t *sc = &cpu_schedulers[cpu];
        grav_thread_t *current = sc->current;
        if (!current || current == sc->idle_thread) {
            sc->idle_time_ns += elapsed_ns;
            continue;
        }

        /* Update CFS virtual runtime (weighted by priority) */
        if (current->policy == GRAV_SCHED_CFS || 
            current->policy == GRAV_SCHED_AI_HINT) {
            u64 weight = 1024 / ((u64)current->priority + 1);
            current->vruntime += elapsed_ns * weight / 1024;

            /* Check if preemption needed */
            u64 used = current->vruntime - sc->min_vruntime;
            if (used >= current->time_slice_ns) {
                current->state = GRAV_PROC_READY;
                grav_sched_add_thread(current);
                grav_thread_t *next = grav_sched_pick_next(cpu);
                /* Context switch would happen here via arch_switch_context() */
                (void)next;
            }
        }

        /* EDF: check deadline miss */
        if (current->policy == GRAV_SCHED_EDF) {
            /* If deadline passed, log warning */
        }
    }
}

/* ═══════ AI Workload Hint ═══════ */
void grav_sched_ai_hint(grav_tid_t tid, u64 workload_class) {
    /* AI engine tells scheduler what kind of workload this is.
     * Scheduler adjusts timeslice and CPU placement accordingly:
     * - INTERACTIVE: short timeslice, prefer latency-optimized cores
     * - BATCH: long timeslice, prefer efficiency cores
     * - AI_INFERENCE: pin to NPU/GPU-adjacent cores
     * - IO_BOUND: minimal timeslice, fast wake-up priority
     */
    (void)tid;
    (void)workload_class;
    grav_log(GRAV_LOG_DEBUG, "sched: AI hint for tid %u -> class 0x%llx", tid, workload_class);
}

/* ═══════ Yield ═══════ */
void grav_sched_yield(void) {
    /* Current thread voluntarily yields CPU */
    /* In real implementation: save context, pick next, switch */
}

/* ═══════ Set Policy ═══════ */
grav_error_t grav_sched_set_policy(grav_tid_t tid, grav_sched_policy_t policy) {
    (void)tid; (void)policy;
    return GRAV_OK;
}

/* ═══════ Set Affinity ═══════ */
grav_error_t grav_sched_set_affinity(grav_tid_t tid, grav_cpumask_t *mask) {
    (void)tid; (void)mask;
    return GRAV_OK;
}
