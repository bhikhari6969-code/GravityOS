/*
 * GravityOS — Public Kernel ABI Header
 * gravity/kernel.h — Core types shared between kernel and userspace
 * This is the ONLY header userspace needs to make syscalls.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _GRAVITY_KERNEL_H
#define _GRAVITY_KERNEL_H

/* ═══════ Primitive Types ═══════ */
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

/* Kernel-specific types */
typedef u64  grav_paddr_t;      /* Physical address */
typedef u64  grav_vaddr_t;      /* Virtual address */
typedef u64  grav_cap_t;        /* Capability token (unforgeable) */
typedef u32  grav_pid_t;        /* Process ID */
typedef u32  grav_tid_t;        /* Thread ID */
typedef i32  grav_err_t;        /* Error code */
typedef u64  grav_size_t;
typedef i64  grav_ssize_t;

#define GRAV_NULL  ((void *)0)

/* ═══════ Error Codes ═══════ */
#define GE_OK           0
#define GE_NOMEM       -1
#define GE_INVAL       -2
#define GE_PERM        -3
#define GE_NOCAP       -4
#define GE_BADCAP      -5
#define GE_NOTFOUND    -6
#define GE_BUSY        -7
#define GE_TIMEOUT     -8
#define GE_IO          -10
#define GE_DEAD        -11
#define GE_EXIST       -12
#define GE_FAULT       -15
#define GE_REVOKED     -16
#define GE_WOULDBLOCK  -17

/* ═══════ Capability Rights ═══════ */
#define GC_READ     (1ULL << 0)
#define GC_WRITE    (1ULL << 1)
#define GC_EXEC     (1ULL << 2)
#define GC_GRANT    (1ULL << 3)
#define GC_REVOKE   (1ULL << 4)
#define GC_MAP      (1ULL << 5)
#define GC_SEND     (1ULL << 6)
#define GC_RECV     (1ULL << 7)
#define GC_CREATE   (1ULL << 8)
#define GC_DESTROY  (1ULL << 9)
#define GC_IRQ      (1ULL << 10)
#define GC_IO       (1ULL << 11)
#define GC_DMA      (1ULL << 12)
#define GC_NET      (1ULL << 13)
#define GC_DISPLAY  (1ULL << 14)
#define GC_AUDIO    (1ULL << 15)
#define GC_ALL      (0xFFFFFFFFFFFFFFFFULL)

/* Capability resource types */
#define GC_TYPE_MEMORY    0x01
#define GC_TYPE_THREAD    0x02
#define GC_TYPE_PROCESS   0x03
#define GC_TYPE_ENDPOINT  0x04
#define GC_TYPE_IRQ       0x05
#define GC_TYPE_DEVICE    0x07
#define GC_TYPE_FILE      0x08
#define GC_TYPE_SOCKET    0x09
#define GC_TYPE_DISPLAY   0x0B

/* ═══════ Memory Flags ═══════ */
#define GM_READ       (1U << 0)
#define GM_WRITE      (1U << 1)
#define GM_EXEC       (1U << 2)
#define GM_USER       (1U << 3)
#define GM_NOCACHE    (1U << 4)
#define GM_DMA        (1U << 6)
#define GM_MMIO       (1U << 7)
#define GM_HUGE_2M    (1U << 8)
#define GM_HUGE_1G    (1U << 9)
#define GM_SHARED     (1U << 10)
#define GM_COW        (1U << 11)
#define GM_GUARD      (1U << 12)

/* Page sizes */
#define GRAV_PAGE_4K  4096ULL
#define GRAV_PAGE_2M  (2ULL << 20)
#define GRAV_PAGE_1G  (1ULL << 30)

/* ═══════ Process / Thread ═══════ */
typedef enum {
    GRAV_PROC_EMBRYO = 0, GRAV_PROC_READY, GRAV_PROC_RUNNING,
    GRAV_PROC_BLOCKED, GRAV_PROC_SUSPENDED, GRAV_PROC_ZOMBIE, GRAV_PROC_DEAD,
} grav_proc_state_t;

typedef enum {
    GRAV_SCHED_CFS = 0, GRAV_SCHED_RT_FIFO, GRAV_SCHED_RT_RR,
    GRAV_SCHED_EDF, GRAV_SCHED_AI_HINT,
} grav_sched_policy_t;

/* ═══════ Architecture ═══════ */
#if defined(__x86_64__) || defined(_M_X64)
    #define GRAV_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define GRAV_ARCH_ARM64  1
#elif defined(__riscv) && (__riscv_xlen == 64)
    #define GRAV_ARCH_RISCV64 1
#endif

#endif /* _GRAVITY_KERNEL_H */
