/*
 * GravityOS — Capability System
 * Every resource is an unforgeable capability token.
 * Replaces UNIX permission bits entirely.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "gravity_core.h"

#define CAP_TABLE_SIZE  65536
static grav_cap_desc_t cap_table[CAP_TABLE_SIZE];
static u32 cap_count = 0;
static u64 cap_entropy_counter = 0xDEADBEEF42424242ULL;

/* Simple PRNG for capability token generation (real impl uses HW RNG) */
static u64 cap_gen_token(void) {
    cap_entropy_counter ^= cap_entropy_counter << 13;
    cap_entropy_counter ^= cap_entropy_counter >> 7;
    cap_entropy_counter ^= cap_entropy_counter << 17;
    return cap_entropy_counter;
}

grav_error_t grav_cap_init(void) {
    for (u32 i = 0; i < CAP_TABLE_SIZE; i++) {
        cap_table[i].token = 0;
        cap_table[i].rights = 0;
        cap_table[i].flags = 0;
    }
    cap_count = 0;
    grav_log(GRAV_LOG_INFO, "cap: initialized %u capability slots", CAP_TABLE_SIZE);
    return GRAV_OK;
}

grav_cap_t grav_cap_create(grav_pid_t owner, u32 type, u64 resource_id, u64 rights) {
    if (cap_count >= CAP_TABLE_SIZE) return 0;
    
    grav_cap_t token = cap_gen_token();
    grav_cap_desc_t *desc = &cap_table[cap_count];
    desc->token = token;
    desc->rights = rights;
    desc->type = type;
    desc->owner_pid = owner;
    desc->resource_id = resource_id;
    desc->parent_token = 0;
    desc->expiry_ns = 0;
    desc->delegation_depth = 8;
    desc->flags = 0;
    cap_count++;
    
    return token;
}

grav_error_t grav_cap_grant(grav_cap_t cap, grav_pid_t target, u64 rights_mask) {
    grav_cap_desc_t *parent = grav_cap_lookup(cap);
    if (!parent) return GRAV_ERR_NOCAP;
    if (parent->flags & GRAV_CAPF_REVOKED) return GRAV_ERR_REVOKED;
    if (!(parent->rights & GRAV_CAP_GRANT)) return GRAV_ERR_PERM;
    if (parent->delegation_depth == 0) return GRAV_ERR_PERM;
    
    /* Create derived capability with restricted rights */
    u64 derived_rights = parent->rights & rights_mask;
    grav_cap_t child = grav_cap_create(target, parent->type, 
                                        parent->resource_id, derived_rights);
    if (!child) return GRAV_ERR_NOMEM;
    
    grav_cap_desc_t *child_desc = grav_cap_lookup(child);
    if (child_desc) {
        child_desc->parent_token = cap;
        child_desc->delegation_depth = parent->delegation_depth - 1;
        child_desc->flags |= GRAV_CAPF_INHERITED;
    }
    
    return GRAV_OK;
}

grav_error_t grav_cap_revoke(grav_cap_t cap) {
    grav_cap_desc_t *desc = grav_cap_lookup(cap);
    if (!desc) return GRAV_ERR_NOCAP;
    desc->flags |= GRAV_CAPF_REVOKED;
    
    /* Cascade revoke: revoke all children derived from this cap */
    for (u32 i = 0; i < cap_count; i++) {
        if (cap_table[i].parent_token == cap) {
            grav_cap_revoke(cap_table[i].token);
        }
    }
    return GRAV_OK;
}

grav_error_t grav_cap_validate(grav_cap_t cap, u64 required_rights) {
    grav_cap_desc_t *desc = grav_cap_lookup(cap);
    if (!desc) return GRAV_ERR_NOCAP;
    if (desc->flags & GRAV_CAPF_REVOKED) return GRAV_ERR_REVOKED;
    if ((desc->rights & required_rights) != required_rights) return GRAV_ERR_PERM;
    /* Check expiry */
    if (desc->expiry_ns != 0) {
        /* In real impl: compare with current time */
    }
    return GRAV_OK;
}

grav_cap_desc_t *grav_cap_lookup(grav_cap_t cap) {
    for (u32 i = 0; i < cap_count; i++) {
        if (cap_table[i].token == cap) return &cap_table[i];
    }
    return GRAV_NULL;
}
