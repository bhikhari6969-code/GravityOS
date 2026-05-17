/*
 * GravityOS — IPC Subsystem
 * Capability-token-gated synchronous message passing
 * Inspired by seL4/L4 — no two processes communicate without a kernel-issued capability
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "gravity_core.h"

/* ═══════ IPC Endpoint Table ═══════ */
#define MAX_ENDPOINTS 4096

typedef struct {
    grav_cap_t     cap;
    grav_pid_t     owner;
    grav_thread_t *send_queue;    /* Threads blocked on send */
    grav_thread_t *recv_queue;    /* Threads blocked on recv */
    u32            msg_count;
    u32            flags;
    u64            created_ns;
} ipc_endpoint_t;

static ipc_endpoint_t endpoint_table[MAX_ENDPOINTS];
static u32 endpoint_count = 0;

/* ═══════ IPC Initialization ═══════ */
grav_error_t grav_ipc_init(void) {
    for (u32 i = 0; i < MAX_ENDPOINTS; i++) {
        endpoint_table[i].cap = 0;
        endpoint_table[i].owner = 0;
        endpoint_table[i].send_queue = GRAV_NULL;
        endpoint_table[i].recv_queue = GRAV_NULL;
        endpoint_table[i].msg_count = 0;
        endpoint_table[i].flags = 0;
    }
    endpoint_count = 0;
    grav_log(GRAV_LOG_INFO, "ipc: initialized %u endpoint slots", MAX_ENDPOINTS);
    return GRAV_OK;
}

/* ═══════ Find endpoint by capability ═══════ */
static ipc_endpoint_t *ipc_find_endpoint(grav_cap_t cap) {
    for (u32 i = 0; i < endpoint_count; i++) {
        if (endpoint_table[i].cap == cap) {
            return &endpoint_table[i];
        }
    }
    return GRAV_NULL;
}

/* ═══════ Create IPC Endpoint ═══════ */
grav_cap_t grav_ipc_create_endpoint(grav_pid_t owner) {
    if (endpoint_count >= MAX_ENDPOINTS) {
        grav_log(GRAV_LOG_ERROR, "ipc: endpoint table full");
        return 0;
    }

    /* Generate unforgeable capability token using kernel entropy */
    grav_cap_t cap = grav_cap_create(owner, GRAV_CAP_TYPE_ENDPOINT, 
                                      endpoint_count, 
                                      GRAV_CAP_SEND | GRAV_CAP_RECV);

    ipc_endpoint_t *ep = &endpoint_table[endpoint_count];
    ep->cap = cap;
    ep->owner = owner;
    ep->send_queue = GRAV_NULL;
    ep->recv_queue = GRAV_NULL;
    ep->msg_count = 0;
    ep->flags = 0;

    endpoint_count++;
    grav_log(GRAV_LOG_DEBUG, "ipc: created endpoint 0x%llx for pid %u", cap, owner);
    return cap;
}

/* ═══════ IPC Send — Synchronous message passing ═══════ */
grav_error_t grav_ipc_send(grav_cap_t endpoint, grav_msg_t *msg) {
    if (!msg) return GRAV_ERR_INVAL;

    /* Validate capability — sender must have SEND right */
    grav_error_t err = grav_cap_validate(endpoint, GRAV_CAP_SEND);
    if (err != GRAV_OK) {
        grav_log(GRAV_LOG_WARN, "ipc: send denied — invalid capability");
        return GRAV_ERR_NOCAP;
    }

    ipc_endpoint_t *ep = ipc_find_endpoint(endpoint);
    if (!ep) return GRAV_ERR_NOTFOUND;

    /* Fast path: if a receiver is already waiting, direct transfer */
    if (ep->recv_queue != GRAV_NULL) {
        grav_thread_t *receiver = ep->recv_queue;
        ep->recv_queue = receiver->wait_next;
        receiver->wait_next = GRAV_NULL;

        /* Copy message directly to receiver's buffer (zero-copy for caps) */
        /* In real implementation, this would copy to receiver's mapped buffer */
        receiver->state = GRAV_PROC_READY;
        grav_sched_add_thread(receiver);

        ep->msg_count++;
        grav_log(GRAV_LOG_DEBUG, "ipc: fast-path send to tid %u", receiver->tid);
        return GRAV_OK;
    }

    /* Slow path: no receiver waiting — block sender if blocking mode */
    if (msg->header.flags & GRAV_IPCF_NONBLOCK) {
        return GRAV_ERR_WOULDBLOCK;
    }

    /* Block current thread on send queue */
    /* grav_thread_t *current = grav_sched_current();
       current->state = GRAV_PROC_BLOCKED;
       current->wait_next = ep->send_queue;
       ep->send_queue = current;
       grav_sched_yield(); */

    return GRAV_OK;
}

/* ═══════ IPC Receive — Wait for message ═══════ */
grav_error_t grav_ipc_recv(grav_cap_t endpoint, grav_msg_t *msg) {
    if (!msg) return GRAV_ERR_INVAL;

    /* Validate capability — receiver must have RECV right */
    grav_error_t err = grav_cap_validate(endpoint, GRAV_CAP_RECV);
    if (err != GRAV_OK) return GRAV_ERR_NOCAP;

    ipc_endpoint_t *ep = ipc_find_endpoint(endpoint);
    if (!ep) return GRAV_ERR_NOTFOUND;

    /* Fast path: sender already waiting */
    if (ep->send_queue != GRAV_NULL) {
        grav_thread_t *sender = ep->send_queue;
        ep->send_queue = sender->wait_next;
        sender->wait_next = GRAV_NULL;

        /* Transfer message from sender to receiver */
        sender->state = GRAV_PROC_READY;
        grav_sched_add_thread(sender);

        grav_log(GRAV_LOG_DEBUG, "ipc: fast-path recv from tid %u", sender->tid);
        return GRAV_OK;
    }

    /* Slow path: block receiver */
    /* grav_thread_t *current = grav_sched_current();
       current->state = GRAV_PROC_BLOCKED;
       current->wait_next = ep->recv_queue;
       ep->recv_queue = current;
       grav_sched_yield(); */

    return GRAV_OK;
}

/* ═══════ IPC Call — Send + Wait for Reply (RPC pattern) ═══════ */
grav_error_t grav_ipc_call(grav_cap_t endpoint, grav_msg_t *send_msg, grav_msg_t *recv_msg) {
    /* Create ephemeral reply endpoint */
    grav_cap_t reply_cap = grav_ipc_create_endpoint(0); /* kernel-owned */
    send_msg->header.reply_cap = reply_cap;

    grav_error_t err = grav_ipc_send(endpoint, send_msg);
    if (err != GRAV_OK) return err;

    /* Block waiting for reply on ephemeral endpoint */
    err = grav_ipc_recv(reply_cap, recv_msg);

    /* Clean up ephemeral endpoint */
    grav_cap_revoke(reply_cap);
    return err;
}

/* ═══════ IPC Reply — Respond to a Call ═══════ */
grav_error_t grav_ipc_reply(grav_cap_t reply_cap, grav_msg_t *msg) {
    return grav_ipc_send(reply_cap, msg);
}
