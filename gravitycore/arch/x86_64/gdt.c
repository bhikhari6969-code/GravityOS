/*
 * GravityOS — x86_64 GDT/TSS Initialization
 * Global Descriptor Table + Task State Segment setup.
 * Sets kernel/user code/data segments and per-CPU TSS.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define GDT_ENTRIES  7
#define MAX_CPUS     256

/* GDT entry (8 bytes for standard, 16 bytes for TSS) */
typedef struct {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  flags_limit;
    u8  base_high;
} __attribute__((packed)) gdt_entry_t;

/* TSS (Task State Segment) for x86_64 */
typedef struct {
    u32 reserved0;
    u64 rsp0;          /* Kernel stack for ring 0 */
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist[7];        /* Interrupt Stack Table entries */
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed)) tss_t;

/* 16-byte TSS descriptor in GDT (for long mode) */
typedef struct {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  flags_limit;
    u8  base_high;
    u32 base_upper;
    u32 reserved;
} __attribute__((packed)) tss_desc_t;

/* GDTR (GDT register) */
typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) gdtr_t;

/* Per-CPU GDT + TSS */
static gdt_entry_t gdt[MAX_CPUS][GDT_ENTRIES + 2]; /* +2 for 16-byte TSS desc */
static tss_t       tss[MAX_CPUS];
static gdtr_t      gdtr[MAX_CPUS];

/* ═══════ Set GDT Entry ═══════ */
static void gdt_set_entry(gdt_entry_t *entry, u32 base, u32 limit,
                           u8 access, u8 flags) {
    entry->limit_low   = limit & 0xFFFF;
    entry->base_low    = base & 0xFFFF;
    entry->base_mid    = (base >> 16) & 0xFF;
    entry->access      = access;
    entry->flags_limit = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    entry->base_high   = (base >> 24) & 0xFF;
}

/* ═══════ Set TSS Descriptor (16 bytes) ═══════ */
static void gdt_set_tss(gdt_entry_t *gdt_base, u32 slot, u64 tss_addr, u32 tss_size) {
    tss_desc_t *desc = (tss_desc_t *)&gdt_base[slot];
    desc->limit_low   = tss_size & 0xFFFF;
    desc->base_low    = tss_addr & 0xFFFF;
    desc->base_mid    = (tss_addr >> 16) & 0xFF;
    desc->access      = 0x89;  /* Present, 64-bit TSS */
    desc->flags_limit = ((tss_size >> 16) & 0x0F);
    desc->base_high   = (tss_addr >> 24) & 0xFF;
    desc->base_upper  = (u32)(tss_addr >> 32);
    desc->reserved    = 0;
}

/* ═══════ Init GDT for CPU ═══════ */
void grav_gdt_init(u32 cpu_id, grav_vaddr_t kernel_stack) {
    if (cpu_id >= MAX_CPUS) return;

    gdt_entry_t *g = gdt[cpu_id];

    /* 0x00: Null descriptor */
    gdt_set_entry(&g[0], 0, 0, 0, 0);

    /* 0x08: Kernel Code (64-bit, ring 0) */
    gdt_set_entry(&g[1], 0, 0xFFFFF, 0x9A, 0xA0);

    /* 0x10: Kernel Data (ring 0) */
    gdt_set_entry(&g[2], 0, 0xFFFFF, 0x92, 0xC0);

    /* 0x18: User Code (64-bit, ring 3) */
    gdt_set_entry(&g[3], 0, 0xFFFFF, 0xFA, 0xA0);

    /* 0x20: User Data (ring 3) */
    gdt_set_entry(&g[4], 0, 0xFFFFF, 0xF2, 0xC0);

    /* TSS setup */
    tss_t *t = &tss[cpu_id];
    t->rsp0 = kernel_stack;        /* Kernel stack for syscalls/interrupts */
    t->iomap_base = sizeof(tss_t); /* No I/O bitmap */

    /* IST entries for dedicated stacks (NMI, DF, MCE) */
    /* t->ist[0] = nmi_stack; */
    /* t->ist[1] = df_stack; */
    /* t->ist[2] = mce_stack; */

    /* 0x28: TSS descriptor (16 bytes, occupies slots 5–6) */
    gdt_set_tss(g, 5, (u64)(usize)t, sizeof(tss_t) - 1);

    /* Load GDTR */
    gdtr[cpu_id].limit = sizeof(gdt_entry_t) * (GDT_ENTRIES + 2) - 1;
    gdtr[cpu_id].base = (u64)(usize)g;

    __asm__ volatile("lgdt %0" :: "m"(gdtr[cpu_id]));

    /* Load TSS (selector 0x28) */
    __asm__ volatile("ltr %%ax" :: "a"(0x28));
}
