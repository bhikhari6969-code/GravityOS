/*
 * GravityOS — Capability System
 * Each process has a CNode (capability node table). Capabilities are
 * unforgeable 64-bit tokens encoding object pointer + rights bitmask.
 * Kernel validates every access.
 *
 * cap_t  cap_alloc(cnode_t*, obj_t*, cap_rights_t)
 * err_t  cap_grant(cap_t src, cnode_t *dst_cnode)
 * void   cap_revoke(cap_t)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define CAP_MAX_CNODES    1024
#define CAP_CNODE_SIZE    256      /* Slots per CNode */

/* Object types that capabilities can reference */
typedef enum {
    OBJ_NONE = 0,
    OBJ_THREAD,
    OBJ_ENDPOINT,     /* IPC endpoint */
    OBJ_MEMORY,       /* Physical page / VMA */
    OBJ_INTERRUPT,    /* IRQ line */
    OBJ_IOPORT,       /* x86 I/O port */
    OBJ_DEVICE,       /* Device MMIO region */
    OBJ_CNODE,        /* CNode (for delegation) */
    OBJ_ADDRESS_SPACE,
    OBJ_NOTIFICATION, /* Notification/signal */
    OBJ_FILE,         /* VFS file descriptor */
    OBJ_SOCKET,       /* Network socket */
} obj_type_t;

/* Object descriptor (generic) */
typedef struct {
    obj_type_t  type;
    void       *ptr;          /* Pointer to actual kernel object */
    u32         refcount;
    u32         id;
} obj_t;

/* Capability rights bitmask */
typedef u32 cap_rights_t;

#define CAP_RIGHT_READ       (1U << 0)
#define CAP_RIGHT_WRITE      (1U << 1)
#define CAP_RIGHT_EXEC       (1U << 2)
#define CAP_RIGHT_GRANT      (1U << 3)  /* Can delegate to others */
#define CAP_RIGHT_REVOKE     (1U << 4)  /* Can revoke children */
#define CAP_RIGHT_MAP        (1U << 5)  /* Can map memory */
#define CAP_RIGHT_SEND       (1U << 6)  /* Can send IPC */
#define CAP_RIGHT_RECV       (1U << 7)  /* Can receive IPC */
#define CAP_RIGHT_DMA        (1U << 8)  /* DMA access */
#define CAP_RIGHT_IRQ        (1U << 9)  /* IRQ registration */
#define CAP_RIGHT_ALL        0x3FF

/* Capability slot in a CNode */
typedef struct cap_slot {
    grav_cap_t      token;        /* The 64-bit unforgeable token */
    obj_t          *object;       /* Referenced kernel object */
    cap_rights_t    rights;       /* Rights bitmask (can only narrow, never widen) */
    u32             state;        /* 0=empty, 1=valid, 2=revoked */
    grav_cap_t      parent_cap;   /* Who granted this capability */
    u32             child_count;  /* How many derived capabilities exist */
    u32             generation;   /* Revocation generation counter */
} cap_slot_t;

/* CNode — per-process capability table */
typedef struct cnode {
    cap_slot_t      slots[CAP_CNODE_SIZE];
    u32             slot_count;
    grav_pid_t      owner;
    u32             active;
} cnode_t;

static cnode_t cnodes[CAP_MAX_CNODES];
static u32 cnode_count = 0;
static u64 cap_token_counter = 0x1000; /* Global token generator */

/* ═══════ Init ═══════ */
grav_err_t grav_cap_init(void) {
    for (u32 i = 0; i < CAP_MAX_CNODES; i++) {
        cnodes[i].active = 0;
        cnodes[i].slot_count = 0;
    }
    cnode_count = 0;
    cap_token_counter = 0x1000;
    return GE_OK;
}

/* ═══════ Create CNode for a process ═══════ */
cnode_t *cnode_create(grav_pid_t owner) {
    if (cnode_count >= CAP_MAX_CNODES) return (cnode_t*)0;
    cnode_t *cn = &cnodes[cnode_count++];
    cn->owner = owner;
    cn->slot_count = 0;
    cn->active = 1;

    for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
        cn->slots[i].state = 0;
        cn->slots[i].object = (obj_t*)0;
        cn->slots[i].rights = 0;
        cn->slots[i].child_count = 0;
        cn->slots[i].generation = 0;
    }

    return cn;
}

/* ═══════ Allocate Capability ═══════ */
grav_cap_t cap_alloc(cnode_t *cn, obj_t *obj, cap_rights_t rights) {
    if (!cn || !cn->active || !obj) return 0;

    /* Find empty slot */
    for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
        if (cn->slots[i].state == 0) {
            cap_slot_t *s = &cn->slots[i];

            /* Generate unforgeable token:
             * bits [63:48] = object type
             * bits [47:32] = generation counter
             * bits [31:16] = slot index
             * bits [15:0]  = rights hash
             */
            cap_token_counter++;
            s->token = ((u64)obj->type << 48) |
                       ((cap_token_counter & 0xFFFF) << 32) |
                       ((u64)i << 16) |
                       (rights & 0xFFFF);

            s->object = obj;
            s->rights = rights;
            s->state = 1;
            s->parent_cap = 0;
            s->child_count = 0;
            s->generation++;

            obj->refcount++;
            cn->slot_count++;

            return s->token;
        }
    }
    return 0; /* CNode full */
}

/* ═══════ Validate Capability ═══════ */
grav_err_t cap_validate(grav_cap_t cap, cap_rights_t required_rights) {
    /* Search all CNodes for this token (in real impl: direct lookup via token structure) */
    for (u32 c = 0; c < cnode_count; c++) {
        if (!cnodes[c].active) continue;
        for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
            cap_slot_t *s = &cnodes[c].slots[i];
            if (s->state == 1 && s->token == cap) {
                /* Check rights */
                if ((s->rights & required_rights) != required_rights)
                    return GE_PERM;
                return GE_OK;
            }
        }
    }
    return GE_NOTFOUND;
}

/* ═══════ Grant (Delegate) Capability ═══════ */
grav_err_t cap_grant(grav_cap_t src_cap, cnode_t *dst_cnode) {
    if (!dst_cnode || !dst_cnode->active) return GE_INVAL;

    /* Find source capability */
    cap_slot_t *src = (cap_slot_t*)0;
    cnode_t *src_cnode = (cnode_t*)0;
    for (u32 c = 0; c < cnode_count; c++) {
        if (!cnodes[c].active) continue;
        for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
            if (cnodes[c].slots[i].state == 1 && cnodes[c].slots[i].token == src_cap) {
                src = &cnodes[c].slots[i];
                src_cnode = &cnodes[c];
                break;
            }
        }
        if (src) break;
    }

    if (!src) return GE_NOTFOUND;

    /* Must have GRANT right to delegate */
    if (!(src->rights & CAP_RIGHT_GRANT)) return GE_PERM;

    /* Create derived capability in destination CNode */
    /* Derived caps can only have EQUAL or FEWER rights (never more) */
    grav_cap_t new_cap = cap_alloc(dst_cnode, src->object,
                                    src->rights & ~CAP_RIGHT_REVOKE);
    if (!new_cap) return GE_NOMEM;

    /* Record parent-child relationship for cascade revocation */
    src->child_count++;

    /* Set parent in the new slot */
    for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
        if (dst_cnode->slots[i].token == new_cap) {
            dst_cnode->slots[i].parent_cap = src_cap;
            break;
        }
    }

    (void)src_cnode;
    return GE_OK;
}

/* ═══════ Revoke Capability (cascade) ═══════ */
void cap_revoke(grav_cap_t cap) {
    /* Find and invalidate this capability */
    for (u32 c = 0; c < cnode_count; c++) {
        if (!cnodes[c].active) continue;
        for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
            cap_slot_t *s = &cnodes[c].slots[i];
            if (s->state == 1 && s->token == cap) {
                /* Revoke this slot */
                s->state = 2; /* revoked */
                if (s->object) {
                    s->object->refcount--;
                    if (s->object->refcount == 0) {
                        /* Object is no longer referenced — can be freed */
                    }
                }
                cnodes[c].slot_count--;

                /* Cascade: revoke all children */
                if (s->child_count > 0) {
                    cap_revoke_children(cap);
                }
                return;
            }
        }
    }
}

/* ═══════ Cascade revocation ═══════ */
static void cap_revoke_children(grav_cap_t parent_cap) {
    /* Find all capabilities whose parent_cap matches */
    for (u32 c = 0; c < cnode_count; c++) {
        if (!cnodes[c].active) continue;
        for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
            cap_slot_t *s = &cnodes[c].slots[i];
            if (s->state == 1 && s->parent_cap == parent_cap) {
                /* Recursively revoke this child and its descendants */
                grav_cap_t child_cap = s->token;
                s->state = 2;
                if (s->object) s->object->refcount--;
                cnodes[c].slot_count--;

                if (s->child_count > 0) {
                    cap_revoke_children(child_cap);
                }
            }
        }
    }
}

/* ═══════ Lookup object from capability ═══════ */
obj_t *cap_lookup(grav_cap_t cap, cap_rights_t required) {
    for (u32 c = 0; c < cnode_count; c++) {
        if (!cnodes[c].active) continue;
        for (u32 i = 0; i < CAP_CNODE_SIZE; i++) {
            cap_slot_t *s = &cnodes[c].slots[i];
            if (s->state == 1 && s->token == cap) {
                if ((s->rights & required) != required) return (obj_t*)0;
                return s->object;
            }
        }
    }
    return (obj_t*)0;
}
