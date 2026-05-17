/*
 * GravityOS — Syscall Number Definitions
 * gravitycore/include/gravity/syscall_nr.h
 *
 * THE COMPLETE KERNEL INTERFACE — 64 SYSCALLS. PERIOD.
 *
 * Why only 64? Linux has 400+. Every syscall is attack surface,
 * compatibility debt, and maintenance cost. GravityCore's entire
 * kernel ABI fits on one page because almost everything is expressed
 * through TWO primitives: IPC and capabilities.
 *
 * Want to open a file? Capability to VFS endpoint → ipc_call().
 * No open(), read(), write(), close() as kernel syscalls — those
 * live in the VFS server in user-space. The kernel just moves
 * the message.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _GRAVITY_SYSCALL_NR_H
#define _GRAVITY_SYSCALL_NR_H

/* ═══════════════════════════════════════════════════════════════════
 *  MEMORY (0–9)
 *  All memory ops are capability-gated. No process can touch memory
 *  it doesn't hold a cap for — not even the kernel's own pages.
 * ═══════════════════════════════════════════════════════════════════ */

/* vaddr_t mem_map(cap_t as, vaddr_t hint, size_t len,
 *                 uint32_t prot, uint32_t flags) */
#define SYS_MEM_MAP          0

/* err_t mem_unmap(cap_t as, vaddr_t addr, size_t len) */
#define SYS_MEM_UNMAP        1

/* err_t mem_protect(cap_t as, vaddr_t addr, size_t len,
 *                   uint32_t new_prot) */
#define SYS_MEM_PROTECT      2

/* err_t mem_query(cap_t as, vaddr_t addr, mem_info_t *out) */
#define SYS_MEM_QUERY        3

/* cap_t mem_share(cap_t as, vaddr_t addr, size_t len,
 *                 cap_rights_t rights) */
#define SYS_MEM_SHARE        4

/* cap_t mem_huge_alloc(size_t len, uint32_t order)
 * For AI model buffers: 2MB/1GB huge pages. */
#define SYS_MEM_HUGE_ALLOC   5

/* err_t mem_pin(cap_t as, vaddr_t addr, size_t len)
 * Pin pages for DMA — prevents page-out. */
#define SYS_MEM_PIN          6

/* err_t mem_unpin(cap_t as, vaddr_t addr, size_t len) */
#define SYS_MEM_UNPIN        7

/* err_t mem_sync(cap_t as, vaddr_t addr, size_t len, uint32_t flags)
 * Flush/invalidate caches. MS_SYNC, MS_ASYNC, MS_INVALIDATE. */
#define SYS_MEM_SYNC         8

/* cap_t mem_cow_clone(cap_t src_as)
 * Clone entire address space with COW. This IS fork(). */
#define SYS_MEM_COW_CLONE    9

/* ═══════════════════════════════════════════════════════════════════
 *  IPC (10–19)
 *  The IPC subsystem IS the system call layer for everything else.
 *  File I/O, networking, display — all go through ipc_call() to
 *  user-space servers. Zero-copy for large payloads via shared
 *  memory capability transfer.
 * ═══════════════════════════════════════════════════════════════════ */

/* err_t ipc_send(cap_t endpoint, ipc_msg_t *msg) */
#define SYS_IPC_SEND         10

/* err_t ipc_recv(cap_t endpoint, ipc_msg_t *out) */
#define SYS_IPC_RECV         11

/* err_t ipc_call(cap_t ep, ipc_msg_t *send, ipc_msg_t *recv)
 * Combined send+recv. THE primary RPC mechanism. */
#define SYS_IPC_CALL         12

/* err_t ipc_reply(ipc_msg_t *reply)
 * Reply to current incoming call (server-side). */
#define SYS_IPC_REPLY        13

/* err_t ipc_reply_recv(ipc_msg_t *reply, cap_t ep, ipc_msg_t *next)
 * Atomic reply-then-wait. The server hot loop:
 *   for (;;) { handle(msg); ipc_reply_recv(&resp, ep, &msg); } */
#define SYS_IPC_REPLY_RECV   14

/* cap_t endpoint_create(uint32_t queue_depth) */
#define SYS_ENDPOINT_CREATE  15

/* cap_t endpoint_badge(cap_t ep, uint64_t badge)
 * Create badged copy — receiver sees badge to identify sender. */
#define SYS_ENDPOINT_BADGE   16

/* err_t notify_send(cap_t notif, uint64_t bits)
 * Lightweight notification (no message body, just bitmask OR). */
#define SYS_NOTIFY_SEND      17

/* uint64_t notify_wait(cap_t notif, uint64_t mask)
 * Block until any masked bit is set. Returns set bits. */
#define SYS_NOTIFY_WAIT      18

/* uint64_t notify_poll(cap_t notif)
 * Non-blocking poll. Returns current bits, clears them. */
#define SYS_NOTIFY_POLL      19

/* ═══════════════════════════════════════════════════════════════════
 *  CAPABILITY (20–29)
 *  The capability system IS the kernel. Every resource is accessed
 *  only through an unforgeable capability token. A process can
 *  never talk to anything it hasn't been explicitly handed a cap
 *  for — not even through timing side-channels (no shared global
 *  kernel state). This is why GravSec enforces zero-trust with
 *  zero performance overhead.
 * ═══════════════════════════════════════════════════════════════════ */

/* err_t cap_grant(cap_t src, cap_t dst_cnode, uint32_t slot) */
#define SYS_CAP_GRANT        20

/* err_t cap_revoke(cap_t cap)
 * Cascade: revokes all derived caps recursively. */
#define SYS_CAP_REVOKE       21

/* err_t cap_copy(cap_t src, cap_t dst_cnode, uint32_t slot) */
#define SYS_CAP_COPY         22

/* err_t cap_query(cap_t cap, cap_info_t *out)
 * Introspect: type, rights, object ID, badge. */
#define SYS_CAP_QUERY        23

/* cap_t cap_mint(cap_t cap, cap_rights_t mask, uint64_t badge)
 * Create narrowed + badged derivative. Rights can only shrink. */
#define SYS_CAP_MINT         24

/* cap_t cnode_create(uint32_t size_bits)
 * Allocate new capability table (2^size_bits slots). */
#define SYS_CNODE_CREATE     25

/* err_t cnode_delete(cap_t cnode) */
#define SYS_CNODE_DELETE     26

/* cap_t irq_claim(uint32_t irq_num)
 * Claim hardware IRQ — returns cap for irq_ack(). */
#define SYS_IRQ_CLAIM        27

/* err_t irq_ack(cap_t irq_cap)
 * Acknowledge IRQ — re-enables delivery. */
#define SYS_IRQ_ACK          28

/* cap_t devmem_map(paddr_t base, size_t len, uint32_t flags)
 * Map physical device MMIO into caller's address space. */
#define SYS_DEVMEM_MAP       29

/* ═══════════════════════════════════════════════════════════════════
 *  THREAD / PROCESS (30–39)
 *  Threads are the unit of scheduling. Processes are just address
 *  spaces + a CNode. thread_create() is spawn. mem_cow_clone()
 *  is fork. There is no exec() — load a new ELF into a fresh AS.
 * ═══════════════════════════════════════════════════════════════════ */

/* cap_t thread_create(cap_t as, vaddr_t entry, vaddr_t stack,
 *                     uint32_t flags) */
#define SYS_THREAD_CREATE    30

/* void thread_exit(int64_t code) __noreturn */
#define SYS_THREAD_EXIT      31

/* err_t thread_wait(cap_t thread, int64_t *exit_code,
 *                   uint64_t timeout_ns) */
#define SYS_THREAD_WAIT      32

/* void thread_yield(void) */
#define SYS_THREAD_YIELD     33

/* err_t thread_prio(cap_t thread, int32_t priority,
 *                   uint32_t sched_class) */
#define SYS_THREAD_PRIO      34

/* err_t thread_suspend(cap_t thread) */
#define SYS_THREAD_SUSPEND   35

/* err_t thread_resume(cap_t thread) */
#define SYS_THREAD_RESUME    36

/* err_t thread_info(cap_t thread, thread_info_t *out) */
#define SYS_THREAD_INFO      37

/* err_t thread_setname(cap_t thread, const char *name, size_t len) */
#define SYS_THREAD_SETNAME   38

/* cap_t process_create(cap_t parent_as, process_params_t *params)
 * Create new address space + root CNode + initial thread. */
#define SYS_PROCESS_CREATE   39

/* ═══════════════════════════════════════════════════════════════════
 *  ASYNC I/O (40–49)
 *  io_uring-inspired submission/completion ring. All bulk I/O
 *  (disk, network, GPU) goes through this. Fixed buffers +
 *  fixed files for zero-copy. DMA mapping is explicit.
 * ═══════════════════════════════════════════════════════════════════ */

/* cap_t io_ring_create(uint32_t depth) */
#define SYS_IO_RING_CREATE   40

/* err_t io_submit(cap_t ring, io_sqe_t *sqes, uint32_t count) */
#define SYS_IO_SUBMIT        41

/* uint32_t io_await(cap_t ring, io_cqe_t *cqes, uint32_t min,
 *                   uint64_t timeout_ns) */
#define SYS_IO_AWAIT         42

/* err_t io_cancel(cap_t ring, uint64_t user_data) */
#define SYS_IO_CANCEL        43

/* err_t io_fixed_buf(cap_t ring, cap_t mem_cap, uint32_t index)
 * Register fixed buffer for zero-copy I/O. */
#define SYS_IO_FIXED_BUF    44

/* err_t io_fixed_file(cap_t ring, cap_t file_cap, uint32_t index)
 * Register fixed file descriptor. */
#define SYS_IO_FIXED_FILE   45

/* cap_t dma_map(cap_t mem_cap, cap_t device_cap, uint32_t flags)
 * Create IOMMU mapping for device DMA. */
#define SYS_DMA_MAP          46

/* err_t dma_unmap(cap_t dma_cap) */
#define SYS_DMA_UNMAP        47

/* cap_t vfs_open(const char *path, uint32_t flags, uint32_t mode)
 * Convenience: sends ipc_call to VFS server. Returns file cap. */
#define SYS_VFS_OPEN         48

/* err_t vfs_close(cap_t file_cap) */
#define SYS_VFS_CLOSE        49

/* ═══════════════════════════════════════════════════════════════════
 *  TIME / PERF (50–57)
 *  All clocks are monotonic by default. Real-time via CLOCK_REAL.
 *  Timer create/arm/disarm for deadline scheduling. Performance
 *  counters for profiling.
 * ═══════════════════════════════════════════════════════════════════ */

/* err_t time_get(clockid_t clock, timespec_t *out) */
#define SYS_TIME_GET         50

/* err_t clock_sleep(clockid_t clock, uint32_t flags,
 *                   const timespec_t *req, timespec_t *rem) */
#define SYS_CLOCK_SLEEP      51

/* cap_t timer_create(clockid_t clock, uint32_t flags) */
#define SYS_TIMER_CREATE     52

/* err_t timer_arm(cap_t timer, uint64_t expiry_ns,
 *                 uint64_t interval_ns) */
#define SYS_TIMER_ARM        53

/* err_t timer_disarm(cap_t timer) */
#define SYS_TIMER_DISARM     54

/* err_t timer_delete(cap_t timer) */
#define SYS_TIMER_DELETE     55

/* cap_t perf_counter(uint32_t event, uint32_t flags) */
#define SYS_PERF_COUNTER     56

/* err_t perf_read(cap_t counter, uint64_t *value) */
#define SYS_PERF_READ        57

/* ═══════════════════════════════════════════════════════════════════
 *  DEBUG / SYSTEM (58–63)
 *  Diagnostic, entropy, system info. debug_log goes straight
 *  to UART — no IPC, no capability check, always works.
 *  debug_break triggers INT3 for kernel debugger.
 * ═══════════════════════════════════════════════════════════════════ */

/* err_t debug_log(const char *msg, size_t len)
 * Direct UART output. No IPC. Always works. Even in panic. */
#define SYS_DEBUG_LOG        58

/* void debug_break(void)
 * Trigger INT3 breakpoint for attached debugger. */
#define SYS_DEBUG_BREAK      59

/* err_t sysinfo(sysinfo_id_t id, void *buf, size_t *size)
 * Query: CPU count, memory size, boot time, kernel version... */
#define SYS_SYSINFO          60

/* err_t entropy_get(void *buf, size_t len)
 * CSPRNG. Uses RDRAND + timing entropy pool. */
#define SYS_ENTROPY_GET      61

/* err_t thread_sigaction(uint32_t sig, sigaction_t *act,
 *                        sigaction_t *old) */
#define SYS_THREAD_SIGACTION 62

/* err_t thread_sigmask(uint32_t how, sigmask_t *set,
 *                      sigmask_t *old) */
#define SYS_THREAD_SIGMASK   63

/* ═══════════════════════════════════════════════════════════════════ */
#define GRAV_NR_SYSCALLS     64
/* ═══════════════════════════════════════════════════════════════════ */

#endif /* _GRAVITY_SYSCALL_NR_H */
