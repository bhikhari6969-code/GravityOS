/*
 * GravityOS — AI Domain Scheduler
 * Isolates CPU cores for exclusive GravMind use.
 * Priority queue for AI inference jobs with deadline scheduling.
 *
 * err_t    ai_domain_init(cpu_mask_t cores)
 * job_id_t ai_domain_submit(ai_job_t *job)
 * void     ai_domain_preempt(job_id_t)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define AI_MAX_JOBS        256
#define AI_MAX_CORES       16

typedef u32 job_id_t;
typedef struct { u64 mask[4]; } cpu_mask_t;

/* AI job descriptor */
typedef struct {
    job_id_t        id;
    u32             priority;       /* Lower = higher priority */
    u64             deadline_ns;    /* Absolute deadline */
    u64             submit_time_ns;
    u64             start_time_ns;
    u64             complete_time_ns;
    u32             assigned_cpu;
    u32             state;          /* 0=queued, 1=running, 2=done, 3=preempted */
    grav_vaddr_t    model_addr;     /* Model weight buffer */
    grav_size_t     model_size;
    grav_vaddr_t    input_addr;
    grav_size_t     input_size;
    grav_vaddr_t    output_addr;
    grav_size_t     output_size;
    grav_cap_t      cap;            /* Required capability */
} ai_job_t;

#define AI_JOB_QUEUED     0
#define AI_JOB_RUNNING    1
#define AI_JOB_DONE       2
#define AI_JOB_PREEMPTED  3

/* AI domain state */
static struct {
    cpu_mask_t      cores;          /* CPUs reserved for AI */
    u32             core_count;
    ai_job_t        jobs[AI_MAX_JOBS];
    u32             job_count;
    job_id_t        next_job_id;
    u32             active_jobs;    /* Currently running */

    /* Per-core: currently running job */
    job_id_t        core_running[AI_MAX_CORES];

    /* Priority queue (min-heap by deadline) */
    u32             heap[AI_MAX_JOBS];
    u32             heap_size;
    u32             initialized;
} ai_domain;

/* ═══════ Init ═══════ */
grav_err_t ai_domain_init(cpu_mask_t cores) {
    ai_domain.cores = cores;
    ai_domain.core_count = 0;
    ai_domain.job_count = 0;
    ai_domain.next_job_id = 1;
    ai_domain.active_jobs = 0;
    ai_domain.heap_size = 0;
    ai_domain.initialized = 1;

    /* Count reserved cores */
    for (u32 w = 0; w < 4; w++) {
        u64 m = cores.mask[w];
        while (m) { ai_domain.core_count += (m & 1); m >>= 1; }
    }

    /* Tell main scheduler to avoid these cores for normal tasks */
    /* grav_sched_exclude_cores(cores); */

    for (u32 i = 0; i < AI_MAX_CORES; i++) {
        ai_domain.core_running[i] = 0;
    }

    return GE_OK;
}

/* ═══════ Heap operations (min-heap by deadline) ═══════ */
static void heap_swap(u32 a, u32 b) {
    u32 tmp = ai_domain.heap[a];
    ai_domain.heap[a] = ai_domain.heap[b];
    ai_domain.heap[b] = tmp;
}

static void heap_push(u32 job_idx) {
    u32 i = ai_domain.heap_size++;
    ai_domain.heap[i] = job_idx;
    /* Bubble up */
    while (i > 0) {
        u32 parent = (i - 1) / 2;
        if (ai_domain.jobs[ai_domain.heap[i]].deadline_ns <
            ai_domain.jobs[ai_domain.heap[parent]].deadline_ns) {
            heap_swap(i, parent);
            i = parent;
        } else break;
    }
}

static u32 heap_pop(void) {
    if (ai_domain.heap_size == 0) return (u32)-1;
    u32 top = ai_domain.heap[0];
    ai_domain.heap_size--;
    if (ai_domain.heap_size > 0) {
        ai_domain.heap[0] = ai_domain.heap[ai_domain.heap_size];
        /* Bubble down */
        u32 i = 0;
        for (;;) {
            u32 left = 2 * i + 1, right = 2 * i + 2, smallest = i;
            if (left < ai_domain.heap_size &&
                ai_domain.jobs[ai_domain.heap[left]].deadline_ns <
                ai_domain.jobs[ai_domain.heap[smallest]].deadline_ns)
                smallest = left;
            if (right < ai_domain.heap_size &&
                ai_domain.jobs[ai_domain.heap[right]].deadline_ns <
                ai_domain.jobs[ai_domain.heap[smallest]].deadline_ns)
                smallest = right;
            if (smallest == i) break;
            heap_swap(i, smallest);
            i = smallest;
        }
    }
    return top;
}

/* ═══════ Submit Job ═══════ */
job_id_t ai_domain_submit(ai_job_t *job) {
    if (!ai_domain.initialized || ai_domain.job_count >= AI_MAX_JOBS)
        return 0;

    u32 idx = ai_domain.job_count++;
    ai_domain.jobs[idx] = *job;
    ai_domain.jobs[idx].id = ai_domain.next_job_id++;
    ai_domain.jobs[idx].state = AI_JOB_QUEUED;

    /* Add to priority queue */
    heap_push(idx);

    /* Try to dispatch immediately if a core is free */
    for (u32 c = 0; c < ai_domain.core_count; c++) {
        if (ai_domain.core_running[c] == 0) {
            u32 next = heap_pop();
            if (next != (u32)-1) {
                ai_domain.jobs[next].state = AI_JOB_RUNNING;
                ai_domain.jobs[next].assigned_cpu = c;
                ai_domain.core_running[c] = ai_domain.jobs[next].id;
                ai_domain.active_jobs++;
            }
            break;
        }
    }

    return ai_domain.jobs[idx].id;
}

/* ═══════ Preempt Job ═══════ */
void ai_domain_preempt(job_id_t id) {
    for (u32 i = 0; i < ai_domain.job_count; i++) {
        if (ai_domain.jobs[i].id == id && ai_domain.jobs[i].state == AI_JOB_RUNNING) {
            ai_domain.jobs[i].state = AI_JOB_PREEMPTED;
            u32 cpu = ai_domain.jobs[i].assigned_cpu;
            ai_domain.core_running[cpu] = 0;
            ai_domain.active_jobs--;

            /* Re-queue for later */
            heap_push(i);

            /* Dispatch next job on freed core */
            u32 next = heap_pop();
            if (next != (u32)-1) {
                ai_domain.jobs[next].state = AI_JOB_RUNNING;
                ai_domain.jobs[next].assigned_cpu = cpu;
                ai_domain.core_running[cpu] = ai_domain.jobs[next].id;
                ai_domain.active_jobs++;
            }
            return;
        }
    }
}
