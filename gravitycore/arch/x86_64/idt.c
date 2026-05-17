/*
 * GravityOS — x86_64 IDT + ISR Stubs
 * Interrupt Descriptor Table + exception/IRQ handlers.
 * Handles CPU exceptions (0–31), hardware IRQs (32–47),
 * and software interrupts (48+).
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define IDT_ENTRIES 256

/* IDT gate descriptor (16 bytes in long mode) */
typedef struct {
    u16 offset_low;
    u16 selector;
    u8  ist;          /* IST index (0 = no IST) */
    u8  type_attr;    /* Present | DPL | Gate Type */
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) idtr_t;

/* Interrupt frame pushed by CPU on exception */
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, error_code;
    u64 rip, cs, rflags, rsp, ss;
} __attribute__((packed)) int_frame_t;

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;

/* External handlers */
extern void irq_dispatch(u32 irq);
extern grav_err_t handle_page_fault(grav_vaddr_t addr, u32 error_code);
extern void gravity_panic(const char *fmt, ...) __attribute__((noreturn));

/* Exception names for debugging */
static const char *exception_names[32] = {
    "Division Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection", "Page Fault", "Reserved",
    "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization Exception", "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved",
};

/* ═══════ Set IDT Entry ═══════ */
static void idt_set_gate(u32 num, u64 handler, u16 selector, u8 ist, u8 type) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].ist         = ist & 0x07;
    idt[num].type_attr   = type;   /* 0x8E = present, ring 0, interrupt gate */
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].reserved    = 0;
}

/* ═══════ C-level interrupt handler ═══════ */
void isr_handler(int_frame_t *frame) {
    u32 int_no = (u32)frame->int_no;

    /* CPU exceptions (0–31) */
    if (int_no < 32) {
        switch (int_no) {
        case 14: /* Page Fault */
            {
                grav_vaddr_t cr2;
                __asm__ volatile("movq %%cr2, %0" : "=r"(cr2));
                grav_err_t err = handle_page_fault(cr2, (u32)frame->error_code);
                if (err != GE_OK) {
                    gravity_panic("unhandled page fault at %x, error=%x, RIP=%x",
                                  cr2, frame->error_code, frame->rip);
                }
                return;
            }

        case 8:  /* Double Fault */
            gravity_panic("double fault! RIP=%x RSP=%x", frame->rip, frame->rsp);

        case 13: /* General Protection Fault */
            gravity_panic("GPF: error=%x RIP=%x CS=%x",
                          frame->error_code, frame->rip, frame->cs);

        case 6:  /* Invalid Opcode */
            gravity_panic("invalid opcode at RIP=%x", frame->rip);

        case 18: /* Machine Check */
            gravity_panic("machine check exception");

        default:
            gravity_panic("CPU exception %d (%s) at RIP=%x",
                          int_no, exception_names[int_no], frame->rip);
        }
    }

    /* Hardware IRQs (32–47 mapped from PIC/APIC) */
    if (int_no >= 32 && int_no < 48) {
        irq_dispatch(int_no - 32);

        /* Send EOI to APIC */
        /* *(volatile u32 *)0xFEE000B0 = 0; */
        return;
    }

    /* Software interrupts / other vectors */
}

/* ═══════ ISR stubs (assembly, generated here as placeholder) ═══════ */
/* In real implementation, these would be in isr_stubs.S:
 *
 * For each vector 0–255:
 *   - Push dummy error code if CPU doesn't push one
 *   - Push interrupt number
 *   - Save all registers
 *   - Call isr_handler
 *   - Restore registers
 *   - iretq
 *
 * Declared as extern for now:
 */
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_8(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_32(void);
/* ... etc for all 256 vectors */

/* ═══════ IDT Init ═══════ */
void grav_idt_init(void) {
    /* Set exception handlers (0–31) */
    /* In real impl, each calls the proper isr_stub_N */
    /* For critical exceptions, use IST for dedicated stacks */

    /* Vectors that push error codes: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30 */

    /* Example entries: */
    /* idt_set_gate(0,  (u64)isr_stub_0,  0x08, 0, 0x8E); */
    /* idt_set_gate(8,  (u64)isr_stub_8,  0x08, 1, 0x8E);  ← IST 1 for DF */
    /* idt_set_gate(13, (u64)isr_stub_13, 0x08, 0, 0x8E); */
    /* idt_set_gate(14, (u64)isr_stub_14, 0x08, 0, 0x8E); */

    /* Hardware IRQ handlers (32–47) */
    /* idt_set_gate(32, (u64)isr_stub_32, 0x08, 0, 0x8E); */

    /* Load IDTR */
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)(usize)idt;
    __asm__ volatile("lidt %0" :: "m"(idtr));
}
