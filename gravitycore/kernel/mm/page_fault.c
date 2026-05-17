/*
 * GravityOS — Page Fault Handler
 * Handles: demand paging, CoW, guard page violations, stack growth, MMIO.
 *
 * err_t handle_page_fault(vaddr_t addr, uint32_t error_code)
 * static err_t do_cow_fault(vma_t*, vaddr_t)
 * static err_t do_demand_page(vma_t*, vaddr_t)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

/* VMA (Virtual Memory Area) — describes one contiguous mapping */
typedef struct vma {
    grav_vaddr_t    start;
    grav_vaddr_t    end;
    u32             flags;        /* GM_READ | GM_WRITE | GM_EXEC | GM_COW | ... */
    u32             type;         /* VMA_ANON, VMA_FILE, VMA_MMIO, VMA_STACK */
    grav_paddr_t    phys_backing; /* For file/device mappings */
    void           *file;        /* Backing file (for mmap'd files) */
    u64             file_offset;
    struct vma     *next;
} vma_t;

#define VMA_ANON   0x01
#define VMA_FILE   0x02
#define VMA_MMIO   0x03
#define VMA_STACK  0x04
#define VMA_GUARD  0x05

/* Page fault error codes (x86-style, abstracted) */
#define PF_PRESENT  (1U << 0)  /* Page was present */
#define PF_WRITE    (1U << 1)  /* Write access */
#define PF_USER     (1U << 2)  /* User-mode access */
#define PF_RSVD     (1U << 3)  /* Reserved bit set in PTE */
#define PF_EXEC     (1U << 4)  /* Instruction fetch */

extern grav_paddr_t pmm_alloc(int order);
extern void pmm_free(grav_paddr_t, int order);

/* ═══════ Find VMA containing address ═══════ */
static vma_t *find_vma(vma_t *vma_list, grav_vaddr_t addr) {
    for (vma_t *v = vma_list; v; v = v->next) {
        if (addr >= v->start && addr < v->end) return v;
    }
    return (vma_t *)0;
}

/* ═══════ Demand Paging — map on first access ═══════ */
static grav_err_t do_demand_page(vma_t *vma, grav_vaddr_t addr) {
    /* Allocate a physical page */
    grav_paddr_t page = pmm_alloc(0);
    if (!page) return GE_NOMEM;

    /* Zero the page (for anonymous mappings) */
    /* In real impl: memset mapped page to 0 */

    /* Install PTE mapping vaddr → paddr */
    /* grav_vmm_map(current_as, addr & ~0xFFF, page, GRAV_PAGE_4K, vma->flags); */

    (void)addr;
    (void)vma;
    return GE_OK;
}

/* ═══════ Copy-on-Write Fault ═══════ */
static grav_err_t do_cow_fault(vma_t *vma, grav_vaddr_t addr) {
    /* 1. Allocate new physical page */
    grav_paddr_t new_page = pmm_alloc(0);
    if (!new_page) return GE_NOMEM;

    /* 2. Copy contents from old page to new page */
    /* In real impl:
     *   old_paddr = pte_to_phys(walk_page_table(addr));
     *   memcpy(phys_to_virt(new_page), phys_to_virt(old_paddr), 4096);
     */

    /* 3. Update PTE to point to new page, mark writable */
    /* grav_vmm_map(current_as, addr & ~0xFFF, new_page, GRAV_PAGE_4K,
     *              vma->flags | GM_WRITE); */

    /* 4. Decrement refcount on old page, free if zero */

    (void)addr;
    (void)vma;
    return GE_OK;
}

/* ═══════ Stack Growth ═══════ */
static grav_err_t do_stack_growth(vma_t *vma, grav_vaddr_t addr) {
    /* Stack grows downward. Extend VMA start to cover the faulting address. */
    grav_vaddr_t new_start = addr & ~(GRAV_PAGE_4K - 1);

    /* Safety check: don't grow beyond 8MB stack limit */
    if (vma->end - new_start > (8ULL << 20)) return GE_NOMEM;

    vma->start = new_start;

    /* Map the new page */
    return do_demand_page(vma, addr);
}

/* ═══════ Main Page Fault Handler ═══════ */
grav_err_t handle_page_fault(grav_vaddr_t addr, u32 error_code) {
    /* Get current process's VMA list */
    /* In real impl: vma_t *vma_list = current_process()->vma_head; */
    vma_t *vma_list = (vma_t *)0; /* Placeholder */
    
    vma_t *vma = find_vma(vma_list, addr);

    /* Case 1: No VMA — check if it's near a stack VMA (stack growth) */
    if (!vma) {
        /* Look for a stack VMA just above the faulting address */
        for (vma_t *v = vma_list; v; v = v->next) {
            if (v->type == VMA_STACK && addr >= v->start - GRAV_PAGE_4K && addr < v->start) {
                return do_stack_growth(v, addr);
            }
        }

        /* No matching VMA at all — SEGFAULT */
        /* deliver_signal(current_thread(), SIGSEGV); */
        return GE_FAULT;
    }

    /* Case 2: Guard page — SEGFAULT (intentional) */
    if (vma->type == VMA_GUARD) {
        /* deliver_signal(current_thread(), SIGSEGV); */
        return GE_FAULT;
    }

    /* Case 3: MMIO region — map device memory */
    if (vma->type == VMA_MMIO) {
        /* Map the physical MMIO address directly */
        /* grav_vmm_map(current_as, addr & ~0xFFF, vma->phys_backing + (addr - vma->start),
         *              GRAV_PAGE_4K, GM_READ | GM_WRITE | GM_NOCACHE); */
        return GE_OK;
    }

    /* Case 4: Write fault on CoW page */
    if ((error_code & PF_WRITE) && (vma->flags & GM_COW)) {
        return do_cow_fault(vma, addr);
    }

    /* Case 5: Page not present — demand paging */
    if (!(error_code & PF_PRESENT)) {
        return do_demand_page(vma, addr);
    }

    /* Case 6: Permission violation — kill process */
    /* deliver_signal(current_thread(), SIGSEGV); */
    return GE_FAULT;
}
