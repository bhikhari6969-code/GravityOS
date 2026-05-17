/*
 * GravityOS — GravityCore Microkernel
 * gravity_core.h — Master Kernel Header
 * Only 4 things in kernel space: scheduler, IPC, VMM, interrupt dispatch
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _GRAVITY_CORE_H
#define _GRAVITY_CORE_H

#include "gravity_types.h"

/* ═══════ Kernel Version ═══════ */
#define GRAVITY_VERSION_MAJOR  0
#define GRAVITY_VERSION_MINOR  1
#define GRAVITY_VERSION_PATCH  0
#define GRAVITY_VERSION_STRING "0.1.0-alpha"
#define GRAVITY_CODENAME       "Singularity"

/* ═══════ Kernel Limits ═══════ */
#define GRAV_MAX_PROCESSES     4096
#define GRAV_MAX_THREADS       16384
#define GRAV_MAX_CAPS_PER_PROC 1024
#define GRAV_MAX_CPUS          256
#define GRAV_MAX_IRQ           256
#define GRAV_SYSCALL_COUNT     64
#define GRAV_KERNEL_STACK_SIZE (16 * 1024)  /* 16KB kernel stack per thread */

/* ═══════ Thread Control Block ═══════ */
typedef struct grav_thread {
    grav_tid_t          tid;
    grav_pid_t          owner_pid;
    grav_proc_state_t   state;
    grav_priority_t     priority;
    grav_sched_policy_t policy;
    grav_cpumask_t      affinity;
    u64                 time_slice_ns;
    u64                 vruntime;        /* CFS virtual runtime */
    u64                 deadline_ns;     /* For EDF scheduling */
    u64                 period_ns;
    grav_vaddr_t        kernel_stack;
    grav_vaddr_t        user_stack;
    void               *arch_context;    /* Architecture-specific CPU state */
    struct grav_thread *next;            /* Scheduler run queue link */
    struct grav_thread *wait_next;       /* Wait queue link */
    u64                 flags;
    u64                 ai_workload_class; /* AI hint for scheduler */
} grav_thread_t;

/* ═══════ Process Control Block ═══════ */
typedef struct grav_process {
    grav_pid_t          pid;
    char                name[64];
    grav_proc_state_t   state;
    struct grav_process *parent;
    grav_vaddr_t        page_table_root; /* CR3 / TTBR0 / SATP */
    grav_cap_desc_t     caps[GRAV_MAX_CAPS_PER_PROC];
    u32                 cap_count;
    grav_thread_t      *threads;
    u32                 thread_count;
    grav_memregion_t   *regions;
    u32                 region_count;
    gravio_ring_t      *io_ring;         /* Per-process GravIO ring */
    u64                 flags;
    u64                 create_time_ns;
    u64                 cpu_time_ns;
} grav_process_t;

/* ═══════ Kernel Subsystem Init ═══════ */

/* Scheduler — hybrid O(1)+CFS with AI workload awareness */
grav_error_t grav_sched_init(void);
grav_error_t grav_sched_add_thread(grav_thread_t *thread);
grav_error_t grav_sched_remove_thread(grav_tid_t tid);
grav_thread_t *grav_sched_pick_next(u32 cpu_id);
void grav_sched_yield(void);
void grav_sched_tick(u64 elapsed_ns);
grav_error_t grav_sched_set_policy(grav_tid_t tid, grav_sched_policy_t policy);
grav_error_t grav_sched_set_affinity(grav_tid_t tid, grav_cpumask_t *mask);
void grav_sched_ai_hint(grav_tid_t tid, u64 workload_class);

/* IPC — Capability-token-gated message passing */
grav_error_t grav_ipc_init(void);
grav_error_t grav_ipc_send(grav_cap_t endpoint, grav_msg_t *msg);
grav_error_t grav_ipc_recv(grav_cap_t endpoint, grav_msg_t *msg);
grav_error_t grav_ipc_call(grav_cap_t endpoint, grav_msg_t *send, grav_msg_t *recv);
grav_error_t grav_ipc_reply(grav_cap_t reply_cap, grav_msg_t *msg);
grav_cap_t   grav_ipc_create_endpoint(grav_pid_t owner);

/* Virtual Memory Manager — 5-level paging, KASLR, huge pages */
grav_error_t grav_vmm_init(void);
grav_vaddr_t grav_vmm_create_address_space(void);
grav_error_t grav_vmm_destroy_address_space(grav_vaddr_t root);
grav_error_t grav_vmm_map(grav_vaddr_t root, grav_vaddr_t vaddr,
                          grav_addr_t paddr, grav_size_t size, u32 flags);
grav_error_t grav_vmm_unmap(grav_vaddr_t root, grav_vaddr_t vaddr, grav_size_t size);
grav_addr_t  grav_vmm_alloc_pages(u32 count, u32 flags);
void         grav_vmm_free_pages(grav_addr_t paddr, u32 count);
grav_error_t grav_vmm_page_fault_handler(grav_vaddr_t fault_addr, u32 error_code);

/* Capability System */
grav_error_t grav_cap_init(void);
grav_cap_t   grav_cap_create(grav_pid_t owner, u32 type, u64 resource_id, u64 rights);
grav_error_t grav_cap_grant(grav_cap_t cap, grav_pid_t target, u64 rights_mask);
grav_error_t grav_cap_revoke(grav_cap_t cap);
grav_error_t grav_cap_validate(grav_cap_t cap, u64 required_rights);
grav_cap_desc_t *grav_cap_lookup(grav_cap_t cap);

/* Interrupt Dispatch */
grav_error_t grav_irq_init(void);
grav_error_t grav_irq_register(u32 irq_num, grav_cap_t handler_cap);
grav_error_t grav_irq_unregister(u32 irq_num);
void         grav_irq_dispatch(u32 irq_num);
void         grav_irq_ack(u32 irq_num);

/* Process Management */
grav_error_t grav_proc_create(const char *name, grav_process_t **out);
grav_error_t grav_proc_destroy(grav_pid_t pid);
grav_process_t *grav_proc_get(grav_pid_t pid);
grav_error_t grav_thread_create(grav_pid_t pid, grav_vaddr_t entry,
                                grav_priority_t prio, grav_thread_t **out);
grav_error_t grav_thread_destroy(grav_tid_t tid);

/* GravIO — Async I/O */
grav_error_t gravio_init(grav_pid_t pid, u32 sq_entries, u32 cq_entries);
grav_error_t gravio_submit(grav_pid_t pid, gravio_sqe_t *sqe);
grav_error_t gravio_complete(grav_pid_t pid, gravio_cqe_t *cqe);

/* Kernel Panic */
GRAV_NORETURN void grav_panic(const char *fmt, ...);

/* Kernel Log */
void grav_log(u32 level, const char *fmt, ...);
#define GRAV_LOG_DEBUG   0
#define GRAV_LOG_INFO    1
#define GRAV_LOG_WARN    2
#define GRAV_LOG_ERROR   3
#define GRAV_LOG_PANIC   4

#endif /* _GRAVITY_CORE_H */
