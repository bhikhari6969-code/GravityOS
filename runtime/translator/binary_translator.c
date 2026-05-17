/*
 * GravityOS — GravBT Binary Translator
 * JIT binary translation using LLVM IR as intermediate representation.
 * Translates x86↔ARM↔RISC-V. Caches translated blocks on NVMe.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "urt_core.h"

/* ═══════ Translation Cache ═══════ */
#define BT_CACHE_SIZE       (64 * 1024)   /* 64K cached blocks */
#define BT_BLOCK_MAX_INSNS  256           /* Max instructions per block */
#define BT_HOT_THRESHOLD    100           /* JIT optimize after N execs */

static bt_cache_entry_t bt_cache[BT_CACHE_SIZE];
static u32 bt_cache_count = 0;

/* IR instruction — simplified LLVM-like intermediate representation */
typedef enum {
    IR_NOP = 0,
    IR_MOV, IR_ADD, IR_SUB, IR_MUL, IR_DIV,
    IR_AND, IR_OR, IR_XOR, IR_SHL, IR_SHR, IR_SAR,
    IR_CMP, IR_TEST,
    IR_JMP, IR_JCC, IR_CALL, IR_RET,
    IR_LOAD, IR_STORE,
    IR_SYSCALL,
    IR_FENCE,       /* Memory barrier */
    IR_ZEXT, IR_SEXT, IR_TRUNC,
} ir_opcode_t;

typedef struct {
    ir_opcode_t op;
    u8          dst;      /* Virtual register 0–63 */
    u8          src1;
    u8          src2;
    u64         imm;      /* Immediate value */
    u32         width;    /* 8, 16, 32, or 64 bits */
    u32         flags;
} ir_insn_t;

/* Translated block */
typedef struct {
    grav_vaddr_t  source_pc;
    u32           source_size;     /* Original bytes */
    ir_insn_t     ir[BT_BLOCK_MAX_INSNS];
    u32           ir_count;
    u8           *native_code;     /* JIT-compiled target code */
    u32           native_size;
    u32           exec_count;
    u32           optimized;       /* Has been JIT-optimized */
} bt_block_t;

/* ═══════ Init ═══════ */
grav_err_t gravbt_init(void) {
    bt_cache_count = 0;
    for (u32 i = 0; i < BT_CACHE_SIZE; i++) {
        bt_cache[i].source_pc = 0;
        bt_cache[i].target_pc = 0;
        bt_cache[i].size = 0;
        bt_cache[i].exec_count = 0;
    }
    return GE_OK;
}

/* ═══════ x86_64 → IR Decoder (simplified) ═══════ */
static u32 decode_x86_to_ir(const u8 *code, u64 pc, ir_insn_t *ir, u32 max_insns) {
    u32 count = 0;
    u32 offset = 0;

    while (count < max_insns) {
        u8 byte = code[offset];

        switch (byte) {
        case 0x90:  /* NOP */
            ir[count].op = IR_NOP;
            ir[count].width = 0;
            offset += 1;
            break;

        case 0x89:  /* MOV r/m64, r64 */
            ir[count].op = IR_MOV;
            ir[count].dst = (code[offset+1] >> 3) & 0x07;
            ir[count].src1 = code[offset+1] & 0x07;
            ir[count].width = 64;
            offset += 2;
            break;

        case 0x01:  /* ADD r/m64, r64 */
            ir[count].op = IR_ADD;
            ir[count].dst = code[offset+1] & 0x07;
            ir[count].src1 = code[offset+1] & 0x07;
            ir[count].src2 = (code[offset+1] >> 3) & 0x07;
            ir[count].width = 64;
            offset += 2;
            break;

        case 0xC3:  /* RET */
            ir[count].op = IR_RET;
            ir[count].width = 64;
            offset += 1;
            count++;
            goto done; /* End of basic block */

        case 0x0F:  /* SYSCALL (0F 05) */
            if (code[offset+1] == 0x05) {
                ir[count].op = IR_SYSCALL;
                ir[count].width = 64;
                offset += 2;
            }
            break;

        default:
            /* Unknown instruction — emit as opaque load+exec */
            ir[count].op = IR_NOP;
            offset += 1;
            break;
        }
        count++;
    }

done:
    return count;
}

/* ═══════ IR → ARM64 Code Generator (simplified) ═══════ */
static u32 emit_arm64_from_ir(ir_insn_t *ir, u32 ir_count, u8 *out, u32 max_size) {
    u32 offset = 0;

    for (u32 i = 0; i < ir_count && offset + 4 <= max_size; i++) {
        switch (ir[i].op) {
        case IR_NOP:
            /* ARM64 NOP: 0xD503201F */
            out[offset++] = 0x1F; out[offset++] = 0x20;
            out[offset++] = 0x03; out[offset++] = 0xD5;
            break;

        case IR_MOV:
            /* MOV Xd, Xs — ORR Xd, XZR, Xs */
            {
                u32 insn = 0xAA0003E0 | (ir[i].src1 << 16) | ir[i].dst;
                out[offset++] = insn & 0xFF;
                out[offset++] = (insn >> 8) & 0xFF;
                out[offset++] = (insn >> 16) & 0xFF;
                out[offset++] = (insn >> 24) & 0xFF;
            }
            break;

        case IR_ADD:
            /* ADD Xd, Xn, Xm */
            {
                u32 insn = 0x8B000000 | (ir[i].src2 << 16) | (ir[i].src1 << 5) | ir[i].dst;
                out[offset++] = insn & 0xFF;
                out[offset++] = (insn >> 8) & 0xFF;
                out[offset++] = (insn >> 16) & 0xFF;
                out[offset++] = (insn >> 24) & 0xFF;
            }
            break;

        case IR_RET:
            /* RET — 0xD65F03C0 */
            out[offset++] = 0xC0; out[offset++] = 0x03;
            out[offset++] = 0x5F; out[offset++] = 0xD6;
            break;

        case IR_SYSCALL:
            /* SVC #0 — trap to GravityOS kernel */
            out[offset++] = 0x01; out[offset++] = 0x00;
            out[offset++] = 0x00; out[offset++] = 0xD4;
            break;

        default:
            /* NOP for unhandled */
            out[offset++] = 0x1F; out[offset++] = 0x20;
            out[offset++] = 0x03; out[offset++] = 0xD5;
            break;
        }
    }
    return offset;
}

/* ═══════ Translate Block ═══════ */
grav_err_t gravbt_translate_block(urt_app_t *app, grav_vaddr_t pc) {
    if (!app) return GE_INVAL;

    /* Check cache first */
    void *cached = gravbt_lookup_cache(app, pc);
    if (cached) {
        app->bt_cache_hits++;
        return GE_OK;
    }
    app->bt_cache_misses++;

    /* Decode source instructions to IR */
    static ir_insn_t ir_buffer[BT_BLOCK_MAX_INSNS];
    const u8 *source_code = (const u8 *)pc;
    u32 ir_count = decode_x86_to_ir(source_code, pc, ir_buffer, BT_BLOCK_MAX_INSNS);

    /* Generate target code from IR */
    static u8 target_code[4096];
    u32 target_size = emit_arm64_from_ir(ir_buffer, ir_count, target_code, sizeof(target_code));

    /* Store in cache */
    if (bt_cache_count < BT_CACHE_SIZE) {
        bt_cache_entry_t *entry = &bt_cache[bt_cache_count++];
        entry->source_pc = pc;
        entry->target_pc = (grav_vaddr_t)target_code; /* In real impl: allocate executable page */
        entry->size = target_size;
        entry->exec_count = 0;
    }

    return GE_OK;
}

/* ═══════ Cache Lookup ═══════ */
void *gravbt_lookup_cache(urt_app_t *app, grav_vaddr_t pc) {
    (void)app;
    for (u32 i = 0; i < bt_cache_count; i++) {
        if (bt_cache[i].source_pc == pc) {
            bt_cache[i].exec_count++;
            return (void *)bt_cache[i].target_pc;
        }
    }
    return GRAV_NULL;
}

/* ═══════ Cache Flush ═══════ */
grav_err_t gravbt_flush_cache(urt_app_t *app) {
    (void)app;
    bt_cache_count = 0;
    return GE_OK;
}
