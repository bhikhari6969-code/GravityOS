/*
 * GravityOS — Syscall Interface
 * 64 syscalls total. No POSIX legacy. All I/O is async via GravIO.
 * Every syscall is capability-gated.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "gravity_core.h"

/* ═══════ The 64 GravityOS Syscalls ═══════
 *
 * Category 0x00-0x0F: Process & Thread
 * Category 0x10-0x1F: Memory
 * Category 0x20-0x2F: IPC & Capabilities  
 * Category 0x30-0x37: GravIO (Async I/O)
 * Category 0x38-0x3B: Time & Clock
 * Category 0x3C-0x3F: System
 */

typedef enum {
    /* Process & Thread (16) */
    SYS_PROC_CREATE     = 0x00,
    SYS_PROC_DESTROY    = 0x01,
    SYS_PROC_INFO       = 0x02,
    SYS_PROC_WAIT       = 0x03,
    SYS_THREAD_CREATE   = 0x04,
    SYS_THREAD_DESTROY  = 0x05,
    SYS_THREAD_YIELD    = 0x06,
    SYS_THREAD_SLEEP    = 0x07,
    SYS_THREAD_SET_PRIO = 0x08,
    SYS_THREAD_SET_AFF  = 0x09,
    SYS_THREAD_SUSPEND  = 0x0A,
    SYS_THREAD_RESUME   = 0x0B,
    SYS_SCHED_SET_POL   = 0x0C,
    SYS_SCHED_AI_HINT   = 0x0D,
    SYS_PROC_EXEC       = 0x0E,
    SYS_PROC_FORK_COW   = 0x0F,

    /* Memory (16) */
    SYS_MEM_MAP         = 0x10,
    SYS_MEM_UNMAP       = 0x11,
    SYS_MEM_PROTECT     = 0x12,
    SYS_MEM_ALLOC       = 0x13,
    SYS_MEM_FREE        = 0x14,
    SYS_MEM_SHARE       = 0x15,
    SYS_MEM_INFO        = 0x16,
    SYS_MEM_LOCK        = 0x17,
    SYS_MEM_ADVISE      = 0x18,  /* Hint: sequential, random, etc. */
    SYS_MEM_HUGE_ALLOC  = 0x19,  /* 2MB/1GB page allocation */
    SYS_MEM_COW_CLONE   = 0x1A,
    SYS_MEM_GUARD       = 0x1B,
    SYS_MEM_DMA_ALLOC   = 0x1C,
    SYS_MEM_CACHE_FLUSH = 0x1D,
    SYS_MEM_PREFETCH    = 0x1E,
    SYS_MEM_QUERY       = 0x1F,

    /* IPC & Capabilities (16) */
    SYS_IPC_SEND        = 0x20,
    SYS_IPC_RECV        = 0x21,
    SYS_IPC_CALL        = 0x22,
    SYS_IPC_REPLY       = 0x23,
    SYS_IPC_CREATE_EP   = 0x24,
    SYS_IPC_DESTROY_EP  = 0x25,
    SYS_CAP_CREATE      = 0x26,
    SYS_CAP_GRANT       = 0x27,
    SYS_CAP_REVOKE      = 0x28,
    SYS_CAP_VALIDATE    = 0x29,
    SYS_CAP_LIST        = 0x2A,
    SYS_CAP_DERIVE      = 0x2B,  /* Create restricted child cap */
    SYS_NOTIFY_SEND     = 0x2C,  /* Async notification */
    SYS_NOTIFY_WAIT     = 0x2D,
    SYS_SIGNAL_SEND     = 0x2E,
    SYS_SIGNAL_WAIT     = 0x2F,

    /* GravIO — Async I/O (8) */
    SYS_IO_SETUP        = 0x30,  /* Create GravIO ring */
    SYS_IO_DESTROY      = 0x31,
    SYS_IO_SUBMIT       = 0x32,  /* Submit SQEs */
    SYS_IO_COMPLETE     = 0x33,  /* Reap CQEs */
    SYS_IO_CANCEL       = 0x34,
    SYS_IO_POLL         = 0x35,
    SYS_IO_REGISTER     = 0x36,  /* Register buffers/files */
    SYS_IO_UNREGISTER   = 0x37,

    /* Time & Clock (4) */
    SYS_CLOCK_GET       = 0x38,
    SYS_CLOCK_SET       = 0x39,  /* Requires capability */
    SYS_TIMER_CREATE    = 0x3A,
    SYS_TIMER_CANCEL    = 0x3B,

    /* System (4) */
    SYS_SYSTEM_INFO     = 0x3C,
    SYS_SYSTEM_LOG      = 0x3D,
    SYS_SYSTEM_REBOOT   = 0x3E,  /* Requires GRAV_CAP_ALL */
    SYS_SYSTEM_DEBUG    = 0x3F,

    GRAV_SYSCALL_MAX    = 0x40   /* 64 total */
} grav_syscall_t;

/* Syscall handler function type */
typedef grav_error_t (*syscall_handler_t)(u64 arg0, u64 arg1, u64 arg2, 
                                          u64 arg3, u64 arg4, u64 arg5);

/* Syscall handler table */
static syscall_handler_t syscall_table[GRAV_SYSCALL_MAX];

/* ═══════ Syscall Implementations ═══════ */

static grav_error_t sys_proc_create(u64 name_ptr, u64 name_len, u64 flags,
                                     u64 parent_cap, u64 unused1, u64 unused2) {
    (void)unused1; (void)unused2;
    grav_error_t err = grav_cap_validate((grav_cap_t)parent_cap, GRAV_CAP_CREATE);
    if (err != GRAV_OK) return GRAV_ERR_NOCAP;
    
    grav_process_t *proc;
    return grav_proc_create((const char *)name_ptr, &proc);
}

static grav_error_t sys_ipc_send(u64 endpoint_cap, u64 msg_ptr, u64 unused1,
                                  u64 unused2, u64 unused3, u64 unused4) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4;
    return grav_ipc_send((grav_cap_t)endpoint_cap, (grav_msg_t *)msg_ptr);
}

static grav_error_t sys_ipc_recv(u64 endpoint_cap, u64 msg_ptr, u64 unused1,
                                  u64 unused2, u64 unused3, u64 unused4) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4;
    return grav_ipc_recv((grav_cap_t)endpoint_cap, (grav_msg_t *)msg_ptr);
}

static grav_error_t sys_mem_map(u64 vaddr, u64 size, u64 flags,
                                 u64 cap, u64 unused1, u64 unused2) {
    (void)unused1; (void)unused2;
    grav_error_t err = grav_cap_validate((grav_cap_t)cap, GRAV_CAP_MAP);
    if (err != GRAV_OK) return GRAV_ERR_NOCAP;
    
    grav_addr_t paddr = grav_vmm_alloc_pages((u32)(size / GRAV_PAGE_4K), (u32)flags);
    if (!paddr) return GRAV_ERR_NOMEM;
    
    return grav_vmm_map(0, (grav_vaddr_t)vaddr, paddr, (grav_size_t)size, (u32)flags);
}

static grav_error_t sys_thread_yield(u64 u1, u64 u2, u64 u3, 
                                      u64 u4, u64 u5, u64 u6) {
    (void)u1;(void)u2;(void)u3;(void)u4;(void)u5;(void)u6;
    grav_sched_yield();
    return GRAV_OK;
}

static grav_error_t sys_io_setup(u64 sq_entries, u64 cq_entries, u64 u3,
                                  u64 u4, u64 u5, u64 u6) {
    (void)u3;(void)u4;(void)u5;(void)u6;
    /* Create GravIO ring for current process */
    return gravio_init(0, (u32)sq_entries, (u32)cq_entries);
}

/* ═══════ Syscall Dispatch ═══════ */
void grav_syscall_init(void) {
    for (u32 i = 0; i < GRAV_SYSCALL_MAX; i++) {
        syscall_table[i] = GRAV_NULL;
    }
    
    syscall_table[SYS_PROC_CREATE]  = sys_proc_create;
    syscall_table[SYS_IPC_SEND]     = sys_ipc_send;
    syscall_table[SYS_IPC_RECV]     = sys_ipc_recv;
    syscall_table[SYS_MEM_MAP]      = sys_mem_map;
    syscall_table[SYS_THREAD_YIELD] = sys_thread_yield;
    syscall_table[SYS_IO_SETUP]     = sys_io_setup;
    
    grav_log(GRAV_LOG_INFO, "syscall: registered %u handlers (of %u total)", 
             6, GRAV_SYSCALL_MAX);
}

grav_error_t grav_syscall_dispatch(u32 syscall_num, u64 a0, u64 a1, 
                                    u64 a2, u64 a3, u64 a4, u64 a5) {
    if (syscall_num >= GRAV_SYSCALL_MAX) return GRAV_ERR_INVAL;
    
    syscall_handler_t handler = syscall_table[syscall_num];
    if (!handler) {
        grav_log(GRAV_LOG_WARN, "syscall: unimplemented syscall 0x%x", syscall_num);
        return GRAV_ERR_INVAL;
    }
    
    return handler(a0, a1, a2, a3, a4, a5);
}
