/*
 * GravityOS — GravityCore Microkernel
 * gravity_types.h — Core Type Definitions
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _GRAVITY_TYPES_H
#define _GRAVITY_TYPES_H

/* Fixed-Width Integer Types */
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;
typedef u64                 usize;
typedef i64                 isize;
typedef u64                 grav_addr_t;
typedef u64                 grav_vaddr_t;
typedef u64                 grav_size_t;
typedef i64                 grav_ssize_t;
typedef u8                  grav_bool_t;
#define GRAV_TRUE           ((grav_bool_t)1)
#define GRAV_FALSE          ((grav_bool_t)0)
#define GRAV_NULL           ((void *)0)

/* Capability Token — unforgeable 64-bit handle */
typedef u64 grav_cap_t;
#define GRAV_CAP_READ       (1ULL << 0)
#define GRAV_CAP_WRITE      (1ULL << 1)
#define GRAV_CAP_EXEC       (1ULL << 2)
#define GRAV_CAP_GRANT      (1ULL << 3)
#define GRAV_CAP_REVOKE     (1ULL << 4)
#define GRAV_CAP_MAP        (1ULL << 5)
#define GRAV_CAP_SEND       (1ULL << 6)
#define GRAV_CAP_RECV       (1ULL << 7)
#define GRAV_CAP_CREATE     (1ULL << 8)
#define GRAV_CAP_DESTROY    (1ULL << 9)
#define GRAV_CAP_IRQ        (1ULL << 10)
#define GRAV_CAP_IO         (1ULL << 11)
#define GRAV_CAP_DMA        (1ULL << 12)
#define GRAV_CAP_NET        (1ULL << 13)
#define GRAV_CAP_DISPLAY    (1ULL << 14)
#define GRAV_CAP_AUDIO      (1ULL << 15)
#define GRAV_CAP_CAMERA     (1ULL << 16)
#define GRAV_CAP_LOCATION   (1ULL << 17)
#define GRAV_CAP_ALL        (0xFFFFFFFFFFFFFFFFULL)

typedef struct {
    grav_cap_t  token;
    u64         rights;
    u32         type;
    u32         owner_pid;
    u64         resource_id;
    u64         parent_token;
    u64         expiry_ns;
    u32         delegation_depth;
    u32         flags;
} grav_cap_desc_t;

#define GRAV_CAP_TYPE_MEMORY   0x01
#define GRAV_CAP_TYPE_THREAD   0x02
#define GRAV_CAP_TYPE_PROCESS  0x03
#define GRAV_CAP_TYPE_ENDPOINT 0x04
#define GRAV_CAP_TYPE_IRQ      0x05
#define GRAV_CAP_TYPE_DEVICE   0x07
#define GRAV_CAP_TYPE_FILE     0x08
#define GRAV_CAP_TYPE_SOCKET   0x09
#define GRAV_CAP_TYPE_DISPLAY  0x0B

/* Process & Thread */
typedef u32 grav_pid_t;
typedef u32 grav_tid_t;

typedef enum {
    GRAV_PROC_EMBRYO = 0, GRAV_PROC_READY, GRAV_PROC_RUNNING,
    GRAV_PROC_BLOCKED, GRAV_PROC_SUSPENDED, GRAV_PROC_ZOMBIE, GRAV_PROC_DEAD,
} grav_proc_state_t;

typedef enum {
    GRAV_PRIO_IDLE = 0, GRAV_PRIO_LOW = 32, GRAV_PRIO_NORMAL = 64,
    GRAV_PRIO_HIGH = 96, GRAV_PRIO_REALTIME = 128, GRAV_PRIO_CRITICAL = 192,
    GRAV_PRIO_KERNEL = 255,
} grav_priority_t;

typedef enum {
    GRAV_SCHED_CFS = 0, GRAV_SCHED_RT_FIFO, GRAV_SCHED_RT_RR,
    GRAV_SCHED_EDF, GRAV_SCHED_AI_HINT,
} grav_sched_policy_t;

typedef struct { u64 mask[4]; } grav_cpumask_t;

/* Memory */
#define GRAV_PAGE_4K    (4096ULL)
#define GRAV_PAGE_2M    (2ULL * 1024 * 1024)
#define GRAV_PAGE_1G    (1ULL * 1024 * 1024 * 1024)

#define GRAV_MEM_READ      (1U << 0)
#define GRAV_MEM_WRITE     (1U << 1)
#define GRAV_MEM_EXEC      (1U << 2)
#define GRAV_MEM_USER      (1U << 3)
#define GRAV_MEM_NOCACHE   (1U << 4)
#define GRAV_MEM_DMA       (1U << 6)
#define GRAV_MEM_MMIO      (1U << 7)
#define GRAV_MEM_HUGE_2M   (1U << 8)
#define GRAV_MEM_HUGE_1G   (1U << 9)
#define GRAV_MEM_SHARED    (1U << 10)
#define GRAV_MEM_COW       (1U << 11)
#define GRAV_MEM_GUARD     (1U << 12)

typedef struct {
    grav_vaddr_t base;
    grav_size_t  length;
    u32          flags;
    grav_cap_t   cap;
} grav_memregion_t;

/* IPC */
#define GRAV_IPC_MAX_INLINE  256
#define GRAV_IPC_MAX_CAPS    8

typedef struct {
    u32        label;
    u32        length;
    u32        cap_count;
    u32        flags;
    grav_cap_t reply_cap;
} grav_msg_header_t;

typedef struct {
    grav_msg_header_t header;
    u8         inline_data[GRAV_IPC_MAX_INLINE];
    grav_cap_t caps[GRAV_IPC_MAX_CAPS];
} grav_msg_t;

#define GRAV_IPCF_NONBLOCK     (1U << 0)
#define GRAV_IPCF_DONATE_SCHED (1U << 1)

/* GravIO — Async I/O (io_uring-inspired) */
typedef enum {
    GRAVIO_NOP = 0, GRAVIO_READ, GRAVIO_WRITE, GRAVIO_FSYNC,
    GRAVIO_POLL, GRAVIO_SENDMSG, GRAVIO_RECVMSG, GRAVIO_ACCEPT,
    GRAVIO_CONNECT, GRAVIO_OPEN, GRAVIO_CLOSE, GRAVIO_STAT,
    GRAVIO_MMAP, GRAVIO_TIMEOUT, GRAVIO_CANCEL, GRAVIO_LINK,
    GRAVIO_UNLINK, GRAVIO_RENAME, GRAVIO_MKDIR, GRAVIO_SPLICE,
} gravio_op_t;

typedef struct {
    u8         opcode;
    u8         flags;
    u16        ioprio;
    grav_cap_t cap;
    u64        off;
    grav_vaddr_t addr;
    u32        len;
    u32        rw_flags;
    u64        user_data;
} gravio_sqe_t;

typedef struct {
    u64 user_data;
    i32 result;
    u32 flags;
} gravio_cqe_t;

/* Error Codes */
typedef i32 grav_error_t;
#define GRAV_OK             0
#define GRAV_ERR_NOMEM     -1
#define GRAV_ERR_INVAL     -2
#define GRAV_ERR_PERM      -3
#define GRAV_ERR_NOCAP     -4
#define GRAV_ERR_BADCAP    -5
#define GRAV_ERR_NOTFOUND  -6
#define GRAV_ERR_BUSY      -7
#define GRAV_ERR_TIMEOUT   -8
#define GRAV_ERR_IO        -10
#define GRAV_ERR_DEAD      -11
#define GRAV_ERR_FAULT     -15
#define GRAV_ERR_REVOKED   -16

/* Architecture Detection */
#if defined(__x86_64__) || defined(_M_X64)
    #define GRAV_ARCH_X86_64 1
    #define GRAV_ARCH_NAME   "x86-64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define GRAV_ARCH_ARM64  1
    #define GRAV_ARCH_NAME   "ARM64"
#elif defined(__riscv) && (__riscv_xlen == 64)
    #define GRAV_ARCH_RISCV64 1
    #define GRAV_ARCH_NAME   "RISC-V 64"
#endif

/* Compiler Attributes */
#define GRAV_PACKED       __attribute__((packed))
#define GRAV_ALIGNED(n)   __attribute__((aligned(n)))
#define GRAV_NORETURN     __attribute__((noreturn))
#define GRAV_INLINE       static inline __attribute__((always_inline))
#define GRAV_LIKELY(x)    __builtin_expect(!!(x), 1)
#define GRAV_UNLIKELY(x)  __builtin_expect(!!(x), 0)

#endif /* _GRAVITY_TYPES_H */
