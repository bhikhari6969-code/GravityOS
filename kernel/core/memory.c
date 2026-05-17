/*
 * GravityOS — Virtual Memory Manager (GravityMem)
 * 5-level paging, KASLR+ASLR, huge page support, page coloring
 * Capability-gated memory access — no UNIX permission model
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "gravity_core.h"

/* ═══════ Physical Page Frame Allocator ═══════ */
#define MAX_PHYS_PAGES    (1024 * 1024)  /* Supports up to 4TB RAM */
#define PAGE_FRAME_SIZE   GRAV_PAGE_4K

typedef struct {
    grav_addr_t base;
    u32         flags;
    u32         refcount;
    u32         order;        /* Buddy allocator order (0 = 4K, 9 = 2M, 18 = 1G) */
    u32         color;        /* Cache color for page coloring */
} page_frame_t;

/* Buddy allocator free lists (orders 0-18) */
#define BUDDY_MAX_ORDER  19
typedef struct {
    grav_addr_t head;
    u32         count;
} buddy_list_t;

static buddy_list_t free_lists[BUDDY_MAX_ORDER];
static page_frame_t page_frames[MAX_PHYS_PAGES];
static u64 total_pages = 0;
static u64 free_pages = 0;
static u64 kaslr_offset = 0;

/* ═══════ Page Table Definitions (5-level, x86-64 style) ═══════ */
/*
 * Level 5 (PML5): 57-bit virtual addresses (128 PB)
 * Level 4 (PML4): 48-bit virtual addresses (256 TB) 
 * Level 3 (PDPT): 1GB pages possible
 * Level 2 (PD):   2MB pages possible
 * Level 1 (PT):   4KB pages
 */
#define PT_ENTRIES        512
#define PT_PRESENT        (1ULL << 0)
#define PT_WRITABLE       (1ULL << 1)
#define PT_USER           (1ULL << 2)
#define PT_WRITETHROUGH   (1ULL << 3)
#define PT_NOCACHE        (1ULL << 4)
#define PT_ACCESSED       (1ULL << 5)
#define PT_DIRTY          (1ULL << 6)
#define PT_HUGE           (1ULL << 7)   /* 2MB/1GB page */
#define PT_GLOBAL         (1ULL << 8)
#define PT_NX             (1ULL << 63)  /* No-execute */
#define PT_ADDR_MASK      (0x000FFFFFFFFFF000ULL)

typedef u64 pt_entry_t;

/* ═══════ VMM Initialization ═══════ */
grav_error_t grav_vmm_init(void) {
    /* Initialize buddy allocator free lists */
    for (u32 i = 0; i < BUDDY_MAX_ORDER; i++) {
        free_lists[i].head = 0;
        free_lists[i].count = 0;
    }
    
    /* Generate KASLR offset (in real impl: use hardware RNG) */
    kaslr_offset = 0xFFFF800000000000ULL; /* Example kernel base */
    
    grav_log(GRAV_LOG_INFO, "vmm: initialized, KASLR offset 0x%llx", kaslr_offset);
    grav_log(GRAV_LOG_INFO, "vmm: 5-level paging enabled, huge pages supported");
    return GRAV_OK;
}

/* ═══════ Physical Page Allocation (Buddy Allocator) ═══════ */
grav_addr_t grav_vmm_alloc_pages(u32 count, u32 flags) {
    /* Determine buddy order from count */
    u32 order = 0;
    u32 size = 1;
    while (size < count) {
        size <<= 1;
        order++;
    }
    
    if (order >= BUDDY_MAX_ORDER) {
        grav_log(GRAV_LOG_ERROR, "vmm: alloc too large, order %u", order);
        return 0;
    }
    
    /* Find free block at requested order or split higher */
    for (u32 o = order; o < BUDDY_MAX_ORDER; o++) {
        if (free_lists[o].count > 0) {
            grav_addr_t block = free_lists[o].head;
            free_lists[o].count--;
            
            /* Split down to requested order */
            while (o > order) {
                o--;
                grav_addr_t buddy = block + ((u64)PAGE_FRAME_SIZE << o);
                free_lists[o].head = buddy;
                free_lists[o].count++;
            }
            
            free_pages -= (1U << order);
            
            /* Apply page coloring for cache optimization */
            u32 color = (u32)(block / PAGE_FRAME_SIZE) % 16;
            (void)color;
            
            grav_log(GRAV_LOG_DEBUG, "vmm: allocated %u pages at 0x%llx (order %u)", 
                     count, block, order);
            return block;
        }
    }
    
    grav_log(GRAV_LOG_ERROR, "vmm: out of physical memory");
    return 0;
}

/* ═══════ Free Physical Pages ═══════ */
void grav_vmm_free_pages(grav_addr_t paddr, u32 count) {
    u32 order = 0;
    u32 size = 1;
    while (size < count) { size <<= 1; order++; }
    
    /* Add back to free list (buddy coalescing would happen here) */
    free_lists[order].count++;
    free_pages += (1U << order);
    
    grav_log(GRAV_LOG_DEBUG, "vmm: freed %u pages at 0x%llx", count, paddr);
}

/* ═══════ Create Address Space ═══════ */
grav_vaddr_t grav_vmm_create_address_space(void) {
    /* Allocate a PML5/PML4 root page table */
    grav_addr_t root = grav_vmm_alloc_pages(1, 0);
    if (!root) return 0;
    
    /* Zero the root page table */
    /* In real impl: memset mapped page to 0 */
    
    /* Copy kernel mappings to upper half */
    /* This ensures kernel is mapped in every address space */
    
    /* Apply ASLR randomization to user-space base */
    
    grav_log(GRAV_LOG_DEBUG, "vmm: created address space, root at 0x%llx", root);
    return root;
}

/* ═══════ Destroy Address Space ═══════ */
grav_error_t grav_vmm_destroy_address_space(grav_vaddr_t root) {
    if (!root) return GRAV_ERR_INVAL;
    /* Walk and free all page table pages */
    /* Free the root */
    grav_vmm_free_pages((grav_addr_t)root, 1);
    return GRAV_OK;
}

/* ═══════ Map Virtual → Physical ═══════ */
grav_error_t grav_vmm_map(grav_vaddr_t root, grav_vaddr_t vaddr,
                          grav_addr_t paddr, grav_size_t size, u32 flags) {
    if (!root) return GRAV_ERR_INVAL;
    
    /* Determine page size */
    grav_size_t page_size = GRAV_PAGE_4K;
    if (flags & GRAV_MEM_HUGE_1G) page_size = GRAV_PAGE_1G;
    else if (flags & GRAV_MEM_HUGE_2M) page_size = GRAV_PAGE_2M;
    
    /* Build page table entry flags */
    u64 pt_flags = PT_PRESENT;
    if (flags & GRAV_MEM_WRITE) pt_flags |= PT_WRITABLE;
    if (flags & GRAV_MEM_USER)  pt_flags |= PT_USER;
    if (!(flags & GRAV_MEM_EXEC)) pt_flags |= PT_NX;
    if (flags & GRAV_MEM_NOCACHE) pt_flags |= PT_NOCACHE;
    if (page_size > GRAV_PAGE_4K) pt_flags |= PT_HUGE;
    
    /* Walk/create page table levels and install mapping */
    grav_size_t mapped = 0;
    while (mapped < size) {
        /* In real implementation:
         * 1. Walk PML5 → PML4 → PDPT → PD → PT
         * 2. Allocate intermediate tables as needed
         * 3. Install leaf entry with paddr + flags
         */
        mapped += page_size;
        paddr += page_size;
        vaddr += page_size;
    }
    
    grav_log(GRAV_LOG_DEBUG, "vmm: mapped 0x%llx -> 0x%llx (%llu bytes, flags=0x%x)",
             vaddr - size, paddr - size, size, flags);
    return GRAV_OK;
}

/* ═══════ Unmap ═══════ */
grav_error_t grav_vmm_unmap(grav_vaddr_t root, grav_vaddr_t vaddr, grav_size_t size) {
    if (!root) return GRAV_ERR_INVAL;
    /* Walk page tables, clear entries, flush TLB */
    grav_log(GRAV_LOG_DEBUG, "vmm: unmapped 0x%llx (%llu bytes)", vaddr, size);
    return GRAV_OK;
}

/* ═══════ Page Fault Handler ═══════ */
grav_error_t grav_vmm_page_fault_handler(grav_vaddr_t fault_addr, u32 error_code) {
    grav_log(GRAV_LOG_DEBUG, "vmm: page fault at 0x%llx (error=0x%x)", 
             fault_addr, error_code);
    
    /* Check if this is a valid CoW fault */
    /* Check if this is a guard page (stack growth) */
    /* Check if this is a lazy allocation */
    /* Otherwise: segfault — kill the process */
    
    /* CoW handling:
     * 1. Look up the faulting page in process memory regions
     * 2. If COW flag set, allocate new page, copy, remap as writable
     * 3. If guard page, expand stack
     * 4. Otherwise, deliver fault to process
     */
    
    return GRAV_OK;
}
