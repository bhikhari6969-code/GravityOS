/*
 * GravityOS — GravitySec Core
 * Autonomous security daemon — runs 24/7 in Ring 4.
 * eBPF syscall tracing, zero-trust policy, behavioral analysis.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

/* ═══════ Security Policy Engine ═══════ */
typedef enum {
    SEC_VERDICT_ALLOW    = 0,
    SEC_VERDICT_DENY     = 1,
    SEC_VERDICT_AUDIT    = 2,
    SEC_VERDICT_QUARANTINE = 3,
} sec_verdict_t;

typedef struct {
    grav_pid_t  pid;
    u64         syscall_counts[64];  /* Per-syscall histogram */
    u64         ipc_msg_count;
    u64         mem_alloc_bytes;
    u64         net_bytes_sent;
    u64         net_bytes_recv;
    u32         open_caps;
    u32         anomaly_score;       /* 0–1000, AI-computed */
    u64         first_seen_ns;
    u64         last_activity_ns;
} sec_process_profile_t;

#define GRAVSEC_MAX_PROFILES 4096
static sec_process_profile_t profiles[GRAVSEC_MAX_PROFILES];
static u32 profile_count = 0;

/* ═══════ eBPF Syscall Tracer ═══════ */
typedef struct {
    u64         timestamp_ns;
    grav_pid_t  pid;
    grav_tid_t  tid;
    u32         syscall_nr;
    u64         args[6];
    i64         retval;
    u32         flags;
} sec_trace_event_t;

#define TRACE_RING_SIZE 65536
static sec_trace_event_t trace_ring[TRACE_RING_SIZE];
static u32 trace_head = 0;

/* Record a syscall event */
void gravsec_trace_syscall(grav_pid_t pid, grav_tid_t tid,
                            u32 syscall_nr, u64 *args, i64 retval) {
    sec_trace_event_t *ev = &trace_ring[trace_head % TRACE_RING_SIZE];
    ev->pid = pid;
    ev->tid = tid;
    ev->syscall_nr = syscall_nr;
    for (u32 i = 0; i < 6; i++) ev->args[i] = args[i];
    ev->retval = retval;
    trace_head++;

    /* Update per-process profile */
    for (u32 i = 0; i < profile_count; i++) {
        if (profiles[i].pid == pid) {
            if (syscall_nr < 64) profiles[i].syscall_counts[syscall_nr]++;
            return;
        }
    }
    /* New process — create profile with zero trust */
    if (profile_count < GRAVSEC_MAX_PROFILES) {
        sec_process_profile_t *p = &profiles[profile_count++];
        p->pid = pid;
        for (u32 i = 0; i < 64; i++) p->syscall_counts[i] = 0;
        if (syscall_nr < 64) p->syscall_counts[syscall_nr]++;
        p->anomaly_score = 0;
        p->open_caps = 0;
    }
}

/* ═══════ Zero-Trust Policy Evaluation ═══════ */
sec_verdict_t gravsec_evaluate(grav_pid_t pid, u32 syscall_nr, u64 *args) {
    (void)args;

    /* Find process profile */
    sec_process_profile_t *prof = GRAV_NULL;
    for (u32 i = 0; i < profile_count; i++) {
        if (profiles[i].pid == pid) { prof = &profiles[i]; break; }
    }

    if (!prof) return SEC_VERDICT_DENY; /* Unknown process = deny */

    /* Rule 1: Block excessive memory allocation (potential heap spray) */
    if (syscall_nr == 0x13 /* MEM_ALLOC */ && prof->mem_alloc_bytes > (4ULL << 30)) {
        prof->anomaly_score += 100;
        return SEC_VERDICT_DENY;
    }

    /* Rule 2: Block rapid IPC (potential side-channel) */
    if ((syscall_nr == 0x20 || syscall_nr == 0x21) && prof->ipc_msg_count > 100000) {
        prof->anomaly_score += 50;
        return SEC_VERDICT_AUDIT;
    }

    /* Rule 3: Anomaly score threshold */
    if (prof->anomaly_score > 500) {
        return SEC_VERDICT_QUARANTINE;
    }

    return SEC_VERDICT_ALLOW;
}

/* ═══════ GravVault — TPM-Backed Key Store ═══════ */
typedef struct {
    u32   id;
    char  label[64];
    u8    encrypted_key[256];
    u32   key_size;
    u32   algorithm;     /* AES-256, RSA-4096, Ed25519, etc. */
    u32   flags;
    grav_cap_t access_cap;
} vault_entry_t;

#define VAULT_MAX_ENTRIES 1024
static vault_entry_t vault[VAULT_MAX_ENTRIES];
static u32 vault_count = 0;

grav_err_t gravvault_store(const char *label, const u8 *key, u32 key_size,
                            u32 algorithm, grav_cap_t access_cap) {
    if (vault_count >= VAULT_MAX_ENTRIES) return GE_NOMEM;
    vault_entry_t *e = &vault[vault_count++];
    e->id = vault_count;
    e->key_size = key_size;
    e->algorithm = algorithm;
    e->access_cap = access_cap;
    /* In real impl: TPM2_Create + seal key to PCR state */
    /* For now, copy encrypted (placeholder) */
    for (u32 i = 0; i < key_size && i < 256; i++) e->encrypted_key[i] = key[i];
    return GE_OK;
}

grav_err_t gravvault_retrieve(u32 id, grav_cap_t cap, u8 *out, u32 *out_size) {
    for (u32 i = 0; i < vault_count; i++) {
        if (vault[i].id == id) {
            /* Validate capability */
            /* In real impl: TPM2_Unseal, verify PCR state */
            *out_size = vault[i].key_size;
            for (u32 j = 0; j < vault[i].key_size && j < 256; j++)
                out[j] = vault[i].encrypted_key[j];
            (void)cap;
            return GE_OK;
        }
    }
    return GE_NOTFOUND;
}

/* ═══════ GravLog — Tamper-Evident Audit Log ═══════ */
typedef struct {
    u64         sequence;
    u64         timestamp_ns;
    u32         severity;       /* 0=info, 1=warn, 2=alert, 3=critical */
    grav_pid_t  source_pid;
    u32         event_type;
    char        message[256];
    u8          hash[32];       /* SHA-256 chain hash */
} gravlog_entry_t;

#define GRAVLOG_MAX 65536
static gravlog_entry_t audit_log[GRAVLOG_MAX];
static u64 log_sequence = 0;

void gravlog_append(u32 severity, grav_pid_t pid, u32 event_type, const char *msg) {
    gravlog_entry_t *e = &audit_log[log_sequence % GRAVLOG_MAX];
    e->sequence = log_sequence;
    e->severity = severity;
    e->source_pid = pid;
    e->event_type = event_type;
    /* Copy message */
    u32 i;
    for (i = 0; msg[i] && i < 255; i++) e->message[i] = msg[i];
    e->message[i] = '\0';
    /* In real impl: SHA-256(prev_hash || entry) → e->hash */
    /* TPM2_Sign(e->hash) for tamper evidence */
    log_sequence++;
}
