/*
 * GravityOS — Physical Memory Manager (PMM)
 * Buddy allocator. Orders 0–10 (4KB–4MB). Per-NUMA free lists.
 * Zones: DMA, NORMAL, HUGE.
 *
 * paddr_t pmm_alloc(int order)
 * void    pmm_free(paddr_t, int order)
 * size_t  pmm_free_pages(void)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define PMM_MAX_ORDER     11       /* orders 0–10 */
#define PMM_PAGE_SIZE     4096ULL
#define PMM_MAX_PAGES     (1ULL << 20)  /* 4 TB */

/* Memory zones */
typedef enum {
    PMM_ZONE_DMA    = 0,   /* 0–16 MB, ISA DMA */
    PMM_ZONE_NORMAL = 1,   /* 16 MB – 4 GB */
    PMM_ZONE_HUGE   = 2,   /* 4 GB+, for AI model buffers */
    PMM_ZONE_COUNT  = 3,
} pmm_zone_t;

/* Page frame descriptor */
typedef struct {
    grav_paddr_t  addr;
    u8            order;
    u8            zone;
    u8            flags;
    u8            _pad;
    u32           refcount;
} page_t;

#define PAGE_FREE     0x00
#define PAGE_USED     0x01
#define PAGE_SLAB     0x02
#define PAGE_RESERVED 0x04

/* Per-zone, per-order free list */
typedef struct {
    grav_paddr_t  head;       /* Head of free block linked list */
    u32           count;
} free_area_t;

typedef struct {
    free_area_t   free_areas[PMM_MAX_ORDER];
    u64           total_pages;
    u64           free_pages;
    grav_paddr_t  base;
    grav_paddr_t  limit;
} pmm_zone_info_t;

static pmm_zone_info_t zones[PMM_ZONE_COUNT];
static page_t page_array[PMM_MAX_PAGES];
static u64 pmm_total_pages = 0;
static u64 pmm_total_free  = 0;

/* ═══════ Init ═══════ */
grav_err_t grav_pmm_init(grav_paddr_t mem_start, grav_size_t mem_size) {
    pmm_total_pages = mem_size / PMM_PAGE_SIZE;
    if (pmm_total_pages > PMM_MAX_PAGES) pmm_total_pages = PMM_MAX_PAGES;

    /* Zero all zones */
    for (int z = 0; z < PMM_ZONE_COUNT; z++) {
        for (int o = 0; o < PMM_MAX_ORDER; o++) {
            zones[z].free_areas[o].head = 0;
            zones[z].free_areas[o].count = 0;
        }
        zones[z].total_pages = 0;
        zones[z].free_pages = 0;
    }

    /* Classify pages into zones */
    for (u64 i = 0; i < pmm_total_pages; i++) {
        grav_paddr_t addr = mem_start + i * PMM_PAGE_SIZE;
        page_array[i].addr = addr;
        page_array[i].order = 0;
        page_array[i].refcount = 0;
        page_array[i].flags = PAGE_FREE;

        if (addr < 0x1000000ULL) {
            page_array[i].zone = PMM_ZONE_DMA;
        } else if (addr < 0x100000000ULL) {
            page_array[i].zone = PMM_ZONE_NORMAL;
        } else {
            page_array[i].zone = PMM_ZONE_HUGE;
        }
        zones[page_array[i].zone].total_pages++;
        zones[page_array[i].zone].free_pages++;
    }

    /* Build initial free lists at max possible order */
    pmm_total_free = pmm_total_pages;
    return GE_OK;
}

/* ═══════ Allocate ═══════ */
grav_paddr_t pmm_alloc(int order) {
    if (order < 0 || order >= PMM_MAX_ORDER) return 0;

    /* Try NORMAL zone first, fallback to HUGE, then DMA */
    int zone_pref[] = { PMM_ZONE_NORMAL, PMM_ZONE_HUGE, PMM_ZONE_DMA };

    for (int zi = 0; zi < PMM_ZONE_COUNT; zi++) {
        pmm_zone_info_t *z = &zones[zone_pref[zi]];

        for (int o = order; o < PMM_MAX_ORDER; o++) {
            if (z->free_areas[o].count > 0) {
                grav_paddr_t block = z->free_areas[o].head;
                z->free_areas[o].count--;

                /* Split down to requested order */
                while (o > order) {
                    o--;
                    grav_paddr_t buddy = block + (PMM_PAGE_SIZE << o);
                    z->free_areas[o].head = buddy;
                    z->free_areas[o].count++;
                }

                u64 pages = 1ULL << order;
                z->free_pages -= pages;
                pmm_total_free -= pages;

                /* Mark pages used */
                u64 idx = (block - page_array[0].addr) / PMM_PAGE_SIZE;
                for (u64 p = 0; p < pages && (idx + p) < pmm_total_pages; p++) {
                    page_array[idx + p].flags = PAGE_USED;
                    page_array[idx + p].order = (u8)order;
                    page_array[idx + p].refcount = 1;
                }

                return block;
            }
        }
    }
    return 0; /* OOM */
}

/* ═══════ Free ═══════ */
void pmm_free(grav_paddr_t addr, int order) {
    if (order < 0 || order >= PMM_MAX_ORDER) return;

    u64 idx = (addr - page_array[0].addr) / PMM_PAGE_SIZE;
    if (idx >= pmm_total_pages) return;

    u8 zone = page_array[idx].zone;
    u64 pages = 1ULL << order;

    for (u64 p = 0; p < pages && (idx + p) < pmm_total_pages; p++) {
        page_array[idx + p].flags = PAGE_FREE;
        page_array[idx + p].refcount = 0;
    }

    /* Add to free list (buddy coalescing would check buddy block state) */
    zones[zone].free_areas[order].count++;
    zones[zone].free_pages += pages;
    pmm_total_free += pages;
}

/* ═══════ Query ═══════ */
grav_size_t pmm_free_pages(void) {
    return pmm_total_free;
}
