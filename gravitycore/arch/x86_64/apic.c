/*
 * GravityOS — x86_64 Local APIC + IOAPIC Driver
 * Replaces legacy 8259 PIC. Supports MSI/MSI-X for PCIe.
 * Per-CPU LAPIC. IOAPIC for external IRQ routing.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

/* LAPIC register offsets (memory-mapped at 0xFEE00000) */
#define LAPIC_BASE          0xFEE00000ULL
#define LAPIC_ID            0x020
#define LAPIC_VERSION       0x030
#define LAPIC_TPR           0x080  /* Task Priority Register */
#define LAPIC_EOI           0x0B0  /* End of Interrupt */
#define LAPIC_SVR           0x0F0  /* Spurious Vector Register */
#define LAPIC_ICR_LOW       0x300  /* Interrupt Command Register */
#define LAPIC_ICR_HIGH      0x310
#define LAPIC_TIMER_LVT     0x320
#define LAPIC_THERMAL_LVT   0x330
#define LAPIC_PERF_LVT      0x340
#define LAPIC_LINT0          0x350
#define LAPIC_LINT1          0x360
#define LAPIC_ERROR_LVT      0x370
#define LAPIC_TIMER_INIT     0x380
#define LAPIC_TIMER_CURRENT  0x390
#define LAPIC_TIMER_DIV      0x3E0

/* IOAPIC register offsets (memory-mapped, base from ACPI MADT) */
#define IOAPIC_BASE          0xFEC00000ULL
#define IOAPIC_REGSEL        0x00
#define IOAPIC_REGWIN        0x10
#define IOAPIC_ID            0x00
#define IOAPIC_VER           0x01
#define IOAPIC_REDTBL_BASE   0x10

/* MMIO read/write */
static inline void lapic_write(u32 reg, u32 value) {
    *(volatile u32 *)(usize)(LAPIC_BASE + reg) = value;
}

static inline u32 lapic_read(u32 reg) {
    return *(volatile u32 *)(usize)(LAPIC_BASE + reg);
}

static inline void ioapic_write(u32 reg, u32 value) {
    *(volatile u32 *)(usize)(IOAPIC_BASE + IOAPIC_REGSEL) = reg;
    *(volatile u32 *)(usize)(IOAPIC_BASE + IOAPIC_REGWIN) = value;
}

static inline u32 ioapic_read(u32 reg) {
    *(volatile u32 *)(usize)(IOAPIC_BASE + IOAPIC_REGSEL) = reg;
    return *(volatile u32 *)(usize)(IOAPIC_BASE + IOAPIC_REGWIN);
}

/* ═══════ LAPIC Init ═══════ */
void grav_lapic_init(void) {
    /* Enable LAPIC via SVR */
    u32 svr = lapic_read(LAPIC_SVR);
    svr |= 0x1FF;  /* Enable + spurious vector 0xFF */
    lapic_write(LAPIC_SVR, svr);

    /* Set task priority to 0 (accept all interrupts) */
    lapic_write(LAPIC_TPR, 0);

    /* Setup LAPIC timer for scheduler ticks */
    lapic_write(LAPIC_TIMER_DIV, 0x03);    /* Divide by 16 */
    lapic_write(LAPIC_TIMER_LVT, 0x20020); /* Vector 32, periodic */

    /* Calibrate timer — measure against known time source (PIT/HPET) */
    /* For now: set a reasonable initial count */
    lapic_write(LAPIC_TIMER_INIT, 1000000); /* ~10ms at typical freq */

    /* Mask unused LVT entries */
    lapic_write(LAPIC_THERMAL_LVT, 0x10000); /* Masked */
    lapic_write(LAPIC_PERF_LVT,    0x10000);
    lapic_write(LAPIC_ERROR_LVT,   0x10000);
}

/* ═══════ Send EOI ═══════ */
void grav_lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

/* ═══════ Get LAPIC ID ═══════ */
u32 grav_lapic_id(void) {
    return lapic_read(LAPIC_ID) >> 24;
}

/* ═══════ Send IPI ═══════ */
void grav_lapic_send_ipi(u32 target_cpu, u32 vector) {
    lapic_write(LAPIC_ICR_HIGH, target_cpu << 24);
    lapic_write(LAPIC_ICR_LOW, vector);
    /* Wait for delivery */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {}
}

/* Broadcast IPI to all CPUs (excluding self) */
void grav_lapic_broadcast_ipi(u32 vector) {
    lapic_write(LAPIC_ICR_HIGH, 0);
    lapic_write(LAPIC_ICR_LOW, vector | (3 << 18)); /* All-excluding-self */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {}
}

/* ═══════ IOAPIC Init ═══════ */
void grav_ioapic_init(void) {
    u32 ver = ioapic_read(IOAPIC_VER);
    u32 max_irq = ((ver >> 16) & 0xFF) + 1;

    /* Mask all IRQ lines initially */
    for (u32 i = 0; i < max_irq; i++) {
        u32 reg = IOAPIC_REDTBL_BASE + i * 2;
        ioapic_write(reg, 0x10000); /* Masked, edge-triggered */
        ioapic_write(reg + 1, 0);
    }
}

/* ═══════ Route IRQ through IOAPIC ═══════ */
void grav_ioapic_route(u32 irq, u32 vector, u32 dest_cpu, u32 flags) {
    u32 reg = IOAPIC_REDTBL_BASE + irq * 2;

    u32 low = vector & 0xFF;
    if (flags & 0x01) low |= (1 << 15);  /* Level-triggered */
    if (flags & 0x02) low |= (1 << 13);  /* Active low */
    /* Delivery mode: fixed (000) */

    ioapic_write(reg, low);
    ioapic_write(reg + 1, dest_cpu << 24);
}

/* ═══════ Unmask IRQ ═══════ */
void grav_ioapic_unmask(u32 irq) {
    u32 reg = IOAPIC_REDTBL_BASE + irq * 2;
    u32 val = ioapic_read(reg);
    val &= ~(1U << 16); /* Clear mask bit */
    ioapic_write(reg, val);
}

/* ═══════ Mask IRQ ═══════ */
void grav_ioapic_mask(u32 irq) {
    u32 reg = IOAPIC_REDTBL_BASE + irq * 2;
    u32 val = ioapic_read(reg);
    val |= (1U << 16);
    ioapic_write(reg, val);
}

/* ═══════ Disable Legacy PIC ═══════ */
void grav_pic_disable(void) {
    /* Mask all IRQs on both 8259 PICs */
    __asm__ volatile("outb %%al, $0x21" :: "a"(0xFF));
    __asm__ volatile("outb %%al, $0xA1" :: "a"(0xFF));
}
