/*
 * GravityOS — IRQ Dispatch
 * Maps hardware IRQs to handler chains. Shared IRQs. Softirqs.
 *
 * err_t irq_request(uint32_t irq, irq_handler_t fn, void *data)
 * void  irq_dispatch(uint32_t irq)
 * void  softirq_raise(uint32_t softirq_id)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define IRQ_MAX         256
#define IRQ_CHAIN_MAX   8     /* Max shared handlers per IRQ */
#define SOFTIRQ_MAX     16

/* IRQ handler function type */
typedef enum { IRQ_NONE = 0, IRQ_HANDLED = 1 } irq_return_t;
typedef irq_return_t (*irq_handler_t)(u32 irq, void *data);

/* Handler chain entry */
typedef struct {
    irq_handler_t   handler;
    void           *data;
    const char     *name;
    u32             flags;
} irq_action_t;

/* Per-IRQ descriptor */
typedef struct {
    irq_action_t    actions[IRQ_CHAIN_MAX];
    u32             action_count;
    u64             trigger_count;
    u32             cpu_affinity;   /* Preferred CPU */
    u32             flags;
    u8              enabled;
} irq_desc_t;

/* Softirq handler */
typedef void (*softirq_handler_t)(void);

static irq_desc_t irq_table[IRQ_MAX];
static softirq_handler_t softirq_handlers[SOFTIRQ_MAX];
static u32 softirq_pending = 0; /* Bitmap of pending softirqs */

/* Well-known softirq IDs */
#define SOFTIRQ_TIMER    0
#define SOFTIRQ_NET_RX   1
#define SOFTIRQ_NET_TX   2
#define SOFTIRQ_BLOCK    3
#define SOFTIRQ_SCHED    4
#define SOFTIRQ_TASKLET  5

/* ═══════ Init ═══════ */
grav_err_t grav_irq_init(void) {
    for (u32 i = 0; i < IRQ_MAX; i++) {
        irq_table[i].action_count = 0;
        irq_table[i].trigger_count = 0;
        irq_table[i].cpu_affinity = 0;
        irq_table[i].flags = 0;
        irq_table[i].enabled = 0;
    }
    for (u32 i = 0; i < SOFTIRQ_MAX; i++) {
        softirq_handlers[i] = (softirq_handler_t)0;
    }
    softirq_pending = 0;
    return GE_OK;
}

/* ═══════ Register IRQ Handler ═══════ */
grav_err_t irq_request(u32 irq, irq_handler_t fn, void *data) {
    if (irq >= IRQ_MAX || !fn) return GE_INVAL;

    irq_desc_t *desc = &irq_table[irq];
    if (desc->action_count >= IRQ_CHAIN_MAX) return GE_NOMEM;

    irq_action_t *action = &desc->actions[desc->action_count];
    action->handler = fn;
    action->data = data;
    action->name = "handler";
    action->flags = 0;
    desc->action_count++;
    desc->enabled = 1;

    return GE_OK;
}

/* ═══════ IRQ Dispatch (called from arch ISR) ═══════ */
void irq_dispatch(u32 irq) {
    if (irq >= IRQ_MAX) return;

    irq_desc_t *desc = &irq_table[irq];
    if (!desc->enabled) return;

    desc->trigger_count++;

    /* Walk handler chain (shared IRQ support) */
    for (u32 i = 0; i < desc->action_count; i++) {
        irq_action_t *action = &desc->actions[i];
        irq_return_t ret = action->handler(irq, action->data);
        if (ret == IRQ_HANDLED) break; /* First handler that claims it wins */
    }

    /* Acknowledge interrupt (arch-specific) */
    /* grav_irq_ack(irq); */
}

/* ═══════ Softirq ═══════ */
void softirq_raise(u32 softirq_id) {
    if (softirq_id >= SOFTIRQ_MAX) return;
    softirq_pending |= (1U << softirq_id);
}

void softirq_register(u32 softirq_id, softirq_handler_t fn) {
    if (softirq_id < SOFTIRQ_MAX) {
        softirq_handlers[softirq_id] = fn;
    }
}

/* Called at the end of every IRQ handler / on return to user */
void softirq_process(void) {
    u32 pending = softirq_pending;
    if (!pending) return;

    for (u32 i = 0; i < SOFTIRQ_MAX; i++) {
        if ((pending & (1U << i)) && softirq_handlers[i]) {
            softirq_pending &= ~(1U << i);
            softirq_handlers[i]();
        }
    }
}
