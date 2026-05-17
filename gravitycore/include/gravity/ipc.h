/*
 * GravityOS — IPC ABI
 * gravity/ipc.h — Wire format for IPC messages
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _GRAVITY_IPC_H
#define _GRAVITY_IPC_H

#include <gravity/kernel.h>

#define GC_IPC_MAX_INLINE  256
#define GC_IPC_MAX_CAPS    8

/* IPC message tag — fits in a register */
typedef struct {
    u32 label;      /* Application-defined message type */
    u16 length;     /* Inline data length (bytes) */
    u8  cap_count;  /* Number of capabilities transferred */
    u8  flags;      /* GC_IPCF_* */
} __attribute__((packed)) gc_ipc_tag_t;

/* Full IPC message */
typedef struct {
    gc_ipc_tag_t  tag;
    grav_cap_t    reply_cap;
    u8            data[GC_IPC_MAX_INLINE];
    grav_cap_t    caps[GC_IPC_MAX_CAPS];
} gc_ipc_msg_t;

/* IPC endpoint descriptor */
typedef struct {
    grav_cap_t  cap;
    grav_pid_t  owner;
    u32         queue_depth;
    u32         flags;
} gc_endpoint_t;

/* IPC flags */
#define GC_IPCF_NONBLOCK       (1U << 0)
#define GC_IPCF_DONATE_SCHED   (1U << 1)
#define GC_IPCF_GRANT          (1U << 2)
#define GC_IPCF_MAP            (1U << 3)

/* Well-known protocol labels */
#define GC_PROTO_VFS_OPEN      0x1001
#define GC_PROTO_VFS_READ      0x1002
#define GC_PROTO_VFS_WRITE     0x1003
#define GC_PROTO_VFS_CLOSE     0x1004
#define GC_PROTO_VFS_STAT      0x1005
#define GC_PROTO_NET_CONNECT   0x2001
#define GC_PROTO_NET_SEND      0x2002
#define GC_PROTO_NET_RECV      0x2003
#define GC_PROTO_DISPLAY_FRAME 0x3001
#define GC_PROTO_AUDIO_WRITE   0x4001
#define GC_PROTO_DEVINIT       0x5001
#define GC_PROTO_DEVIO         0x5002

#endif /* _GRAVITY_IPC_H */
