/*
 * GravityOS — Virtual Memory Manager (VMM)
 * Manages VMAs (virtual memory areas) per address space.
 * Handles mmap-style mappings, CoW faults, huge-page promotion.
 *
 * vma_t  *vma_create(as_t*, vaddr_t, size_t, uint32_t flags)
 * err_t   vma_map_phys(vma_t*, paddr_t)
 * void    vma_destroy(vma_t*)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define VMM_MAX_ADDR_SPACES  1024
#define VMM_MAX_VMAS         16384
#define VMM_HUGE_THRESHOLD   (2ULL << 20)  /* 2MB: promote to huge page */

/* Address space descriptor */
typedef struct as {
    u32             id;
    grav_paddr_t    pgd;        /* Page Global Directory physical address */
    u32             vma_count;
    struct vma     *vma_head;   /* Sorted linked list of VMAs */
    u64             total_mapped;
    u32             refcount;
} as_t;

/* Virtual Memory Area */
typedef struct vma {
    grav_vaddr_t    start;
    grav_vaddr_t    end;        /* Exclusive */
    u32             flags;      /* GM_READ | GM_WRITE | GM_EXEC | GM_COW | GM_USER */
    u32             type;       /* VMA_ANON, VMA_FILE, VMA_MMIO, VMA_STACK */
    grav_paddr_t    phys_base;  /* Backing physical address (for mapped regions) */
    u64             file_offset;
    void           *file;       /* Backing file (for mmap'd files) */
    as_t           *owner;
    struct vma     *next;
    struct vma     *prev;
    u32             refcount;   /* For shared mappings (fork COW) */
} vma_t;

#define VMA_ANON   0x01
#define VMA_FILE   0x02
#define VMA_MMIO   0x03
#define VMA_STACK  0x04

static as_t address_spaces[VMM_MAX_ADDR_SPACES];
static u32 as_count = 0;
static vma_t vma_pool[VMM_MAX_VMAS];
static u32 vma_pool_next = 0;

/* External: page allocator */
extern grav_paddr_t pmm_alloc(int order);
extern void pmm_free(grav_paddr_t, int order);

/* ═══════ Init ═══════ */
grav_err_t grav_vmm_init(void) {
    as_count = 0;
    vma_pool_next = 0;
    for (u32 i = 0; i < VMM_MAX_ADDR_SPACES; i++) {
        address_spaces[i].refcount = 0;
    }
    return GE_OK;
}

/* ═══════ Address Space Management ═══════ */
as_t *as_create(void) {
    if (as_count >= VMM_MAX_ADDR_SPACES) return (as_t*)0;
    as_t *a = &address_spaces[as_count];
    a->id = as_count++;
    a->vma_head = (vma_t*)0;
    a->vma_count = 0;
    a->total_mapped = 0;
    a->refcount = 1;

    /* Allocate top-level page table (PGD) */
    a->pgd = pmm_alloc(0); /* 1 page for PGD */
    /* Zero the page table */
    /* memset(phys_to_virt(a->pgd), 0, 4096); */

    return a;
}

as_t *as_fork(as_t *parent) {
    as_t *child = as_create();
    if (!child) return (as_t*)0;

    /* Copy all VMAs, mark writable ones as COW */
    for (vma_t *v = parent->vma_head; v; v = v->next) {
        vma_t *cv = vma_create(child, v->start, v->end - v->start, v->flags);
        if (!cv) continue;
        cv->type = v->type;
        cv->phys_base = v->phys_base;
        cv->file = v->file;
        cv->file_offset = v->file_offset;

        /* Mark both parent and child as COW for writable pages */
        if (v->flags & GM_WRITE) {
            v->flags |= GM_COW;
            v->flags &= ~GM_WRITE;
            cv->flags |= GM_COW;
            cv->flags &= ~GM_WRITE;
        }

        /* Share physical pages (increment refcount) */
        v->refcount++;
        cv->refcount = v->refcount;
    }

    /* Copy page tables (pointing to same physical pages) */
    /* In real impl: walk parent's page tables, copy PTEs to child */

    return child;
}

void as_destroy(as_t *a) {
    if (!a) return;
    a->refcount--;
    if (a->refcount > 0) return;

    /* Free all VMAs */
    vma_t *v = a->vma_head;
    while (v) {
        vma_t *next = v->next;
        vma_destroy(v);
        v = next;
    }

    /* Free page tables */
    if (a->pgd) pmm_free(a->pgd, 0);
    a->pgd = 0;
}

/* ═══════ VMA Management ═══════ */

/* Check for VMA overlap */
static int vma_overlaps(as_t *a, grav_vaddr_t start, grav_size_t size) {
    grav_vaddr_t end = start + size;
    for (vma_t *v = a->vma_head; v; v = v->next) {
        if (start < v->end && end > v->start) return 1;
    }
    return 0;
}

/* Create a new VMA in the address space */
vma_t *vma_create(as_t *a, grav_vaddr_t start, grav_size_t size, u32 flags) {
    if (!a || size == 0) return (vma_t*)0;

    /* Page-align */
    start &= ~(GRAV_PAGE_4K - 1);
    size = (size + GRAV_PAGE_4K - 1) & ~(GRAV_PAGE_4K - 1);

    /* Check for overlap */
    if (vma_overlaps(a, start, size)) return (vma_t*)0;

    /* Allocate VMA from pool */
    if (vma_pool_next >= VMM_MAX_VMAS) return (vma_t*)0;
    vma_t *v = &vma_pool[vma_pool_next++];

    v->start = start;
    v->end = start + size;
    v->flags = flags;
    v->type = VMA_ANON;
    v->phys_base = 0;
    v->file = (void*)0;
    v->file_offset = 0;
    v->owner = a;
    v->refcount = 1;
    v->next = (vma_t*)0;
    v->prev = (vma_t*)0;

    /* Insert sorted by start address */
    if (!a->vma_head || start < a->vma_head->start) {
        v->next = a->vma_head;
        if (a->vma_head) a->vma_head->prev = v;
        a->vma_head = v;
    } else {
        vma_t *cur = a->vma_head;
        while (cur->next && cur->next->start < start)
            cur = cur->next;
        v->next = cur->next;
        v->prev = cur;
        if (cur->next) cur->next->prev = v;
        cur->next = v;
    }

    a->vma_count++;
    a->total_mapped += size;
    return v;
}

/* Map VMA to physical address */
grav_err_t vma_map_phys(vma_t *v, grav_paddr_t phys) {
    if (!v) return GE_INVAL;

    v->phys_base = phys;
    grav_size_t size = v->end - v->start;

    /* Determine page size — promote to huge page for large AI model buffers */
    if (size >= VMM_HUGE_THRESHOLD && (v->start & ((2ULL << 20) - 1)) == 0) {
        /* Use 2MB huge pages */
        grav_size_t offset = 0;
        while (offset < size) {
            /* Install 2MB PTE:
             * walk_page_table(v->owner->pgd, v->start + offset, 
             *                  CREATE | HUGE_2MB);
             * set_pte(pte, phys + offset, v->flags | PTE_HUGE);
             */
            offset += 2ULL << 20;
        }
    } else {
        /* Use 4KB pages */
        grav_size_t offset = 0;
        while (offset < size) {
            /* Install 4KB PTE:
             * walk_page_table(v->owner->pgd, v->start + offset, CREATE);
             * set_pte(pte, phys + offset, v->flags);
             */
            offset += GRAV_PAGE_4K;
        }
    }

    return GE_OK;
}

/* Destroy a VMA */
void vma_destroy(vma_t *v) {
    if (!v) return;

    as_t *a = v->owner;
    grav_size_t size = v->end - v->start;

    /* Unlink from list */
    if (v->prev) v->prev->next = v->next;
    else if (a) a->vma_head = v->next;
    if (v->next) v->next->prev = v->prev;

    /* Free physical pages if owned */
    v->refcount--;
    if (v->refcount == 0 && v->phys_base) {
        /* Walk page tables, free each physical page */
        grav_size_t offset = 0;
        while (offset < size) {
            /* paddr_t pa = pte_to_phys(walk_page_table(...));
             * pmm_free(pa, 0);
             */
            offset += GRAV_PAGE_4K;
        }
    }

    /* Invalidate TLB entries */
    /* arch_tlb_flush_range(v->start, v->end); */

    if (a) {
        a->vma_count--;
        a->total_mapped -= size;
    }

    /* Zero VMA (return to pool — simplified) */
    v->start = 0;
    v->end = 0;
    v->owner = (as_t*)0;
}

/* ═══════ mmap-style interface ═══════ */
grav_vaddr_t vmm_mmap(as_t *a, grav_vaddr_t hint, grav_size_t size,
                       u32 prot, u32 flags, void *file, u64 offset) {
    /* Find free region if hint is 0 */
    grav_vaddr_t addr = hint;
    if (addr == 0) {
        /* Search for free gap in address space */
        addr = 0x400000; /* Default start above 4MB */
        for (vma_t *v = a->vma_head; v; v = v->next) {
            if (addr + size <= v->start) break;
            addr = v->end;
        }
    }

    vma_t *v = vma_create(a, addr, size, prot);
    if (!v) return 0;

    if (file) {
        v->type = VMA_FILE;
        v->file = file;
        v->file_offset = offset;
    }

    (void)flags;
    return v->start;
}

/* ═══════ munmap ═══════ */
grav_err_t vmm_munmap(as_t *a, grav_vaddr_t addr, grav_size_t size) {
    for (vma_t *v = a->vma_head; v; v = v->next) {
        if (v->start == addr && (v->end - v->start) == size) {
            vma_destroy(v);
            return GE_OK;
        }
    }
    return GE_NOTFOUND;
}
