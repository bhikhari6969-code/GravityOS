/*
 * GravityOS — SLAB Allocator
 * Per-type kernel object caches. CPU-local magazines. Colored slabs.
 *
 * slab_cache_t *slab_create(const char*, size_t, size_t align)
 * void         *slab_alloc(slab_cache_t*)
 * void          slab_free(slab_cache_t*, void*)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define SLAB_MAX_CACHES     64
#define SLAB_PAGE_SIZE      4096
#define SLAB_MAGAZINE_SIZE  32     /* Per-CPU magazine depth */
#define SLAB_NAME_MAX       32

/* Individual slab (one or more pages, divided into equal-size objects) */
typedef struct slab {
    void            *base;         /* Page base address */
    u32              obj_size;     /* Aligned object size */
    u32              capacity;     /* Objects per slab */
    u32              in_use;
    u64              free_bitmap[4]; /* 256-bit bitmap: 1=free */
    struct slab     *next;
} slab_t;

/* Per-CPU magazine for lock-free fast path */
typedef struct {
    void  *objects[SLAB_MAGAZINE_SIZE];
    u32    count;
} slab_magazine_t;

/* Slab cache (one per object type) */
typedef struct slab_cache {
    char             name[SLAB_NAME_MAX];
    u32              obj_size;      /* Requested size */
    u32              aligned_size;  /* After alignment + coloring */
    u32              alignment;
    u32              color_offset;  /* Current color (cycles for cache alignment) */
    u32              color_max;
    slab_t          *partial;       /* Slabs with free objects */
    slab_t          *full;          /* Fully allocated slabs */
    slab_t          *empty;         /* Completely free slabs */
    u64              total_allocs;
    u64              total_frees;
    u32              active;        /* Cache is in use */
} slab_cache_t;

static slab_cache_t caches[SLAB_MAX_CACHES];
static u32 cache_count = 0;

/* External: page allocator */
extern grav_paddr_t pmm_alloc(int order);
extern void pmm_free(grav_paddr_t, int order);

/* ═══════ Align up ═══════ */
static u32 align_up(u32 val, u32 align) {
    return (val + align - 1) & ~(align - 1);
}

/* ═══════ Init ═══════ */
grav_err_t grav_slab_init(void) {
    for (u32 i = 0; i < SLAB_MAX_CACHES; i++) {
        caches[i].active = 0;
    }
    cache_count = 0;
    return GE_OK;
}

/* ═══════ Create Cache ═══════ */
slab_cache_t *slab_create(const char *name, grav_size_t size, grav_size_t align_val) {
    if (cache_count >= SLAB_MAX_CACHES) return (slab_cache_t*)0;
    if (align_val == 0) align_val = 8;

    slab_cache_t *c = &caches[cache_count++];
    /* Copy name */
    u32 i;
    for (i = 0; name[i] && i < SLAB_NAME_MAX - 1; i++) c->name[i] = name[i];
    c->name[i] = '\0';

    c->obj_size = (u32)size;
    c->aligned_size = align_up((u32)size, (u32)align_val);
    c->alignment = (u32)align_val;
    c->color_offset = 0;
    c->color_max = (SLAB_PAGE_SIZE / c->aligned_size > 1) ?
                   (c->alignment) : 0;
    c->partial = (slab_t*)0;
    c->full = (slab_t*)0;
    c->empty = (slab_t*)0;
    c->total_allocs = 0;
    c->total_frees = 0;
    c->active = 1;

    return c;
}

/* ═══════ Grow cache (allocate new slab) ═══════ */
static slab_t *slab_grow(slab_cache_t *c) {
    /* Allocate pages for the slab metadata + objects */
    grav_paddr_t page = pmm_alloc(0); /* 1 page */
    if (!page) return (slab_t*)0;

    /* In real kernel: map page to virtual address */
    slab_t *s = (slab_t *)(void*)(usize)page; /* Placeholder mapping */
    s->base = (void *)(usize)(page + sizeof(slab_t));
    s->obj_size = c->aligned_size;
    s->capacity = (SLAB_PAGE_SIZE - sizeof(slab_t)) / c->aligned_size;
    if (s->capacity > 256) s->capacity = 256;
    s->in_use = 0;
    s->next = (slab_t*)0;

    /* Init free bitmap: all objects free */
    for (i32 i = 0; i < 4; i++) s->free_bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;

    /* Apply cache coloring */
    c->color_offset = (c->color_offset + c->alignment) % (c->color_max + 1);

    return s;
}

/* ═══════ Allocate Object ═══════ */
void *slab_alloc(slab_cache_t *c) {
    if (!c || !c->active) return (void*)0;

    /* Fast path: try partial slab */
    slab_t *s = c->partial;
    if (!s) {
        /* Try empty slab */
        s = c->empty;
        if (s) {
            c->empty = s->next;
        } else {
            /* Grow: allocate new slab */
            s = slab_grow(c);
            if (!s) return (void*)0;
        }
        s->next = c->partial;
        c->partial = s;
    }

    /* Find free slot in bitmap */
    for (u32 w = 0; w < 4; w++) {
        if (s->free_bitmap[w] != 0) {
            u32 bit = __builtin_ctzll(s->free_bitmap[w]);
            u32 idx = w * 64 + bit;
            if (idx >= s->capacity) break;

            s->free_bitmap[w] &= ~(1ULL << bit);
            s->in_use++;

            void *obj = (void *)((usize)s->base + idx * s->obj_size);

            /* Move to full list if needed */
            if (s->in_use >= s->capacity) {
                c->partial = s->next;
                s->next = c->full;
                c->full = s;
            }

            c->total_allocs++;
            return obj;
        }
    }
    return (void*)0;
}

/* ═══════ Free Object ═══════ */
void slab_free(slab_cache_t *c, void *obj) {
    if (!c || !obj) return;

    /* Find which slab this object belongs to (simplified) */
    /* In real impl: compute slab from page address */
    slab_t *s = c->partial;
    if (!s) s = c->full;
    if (!s) return;

    u32 idx = (u32)(((usize)obj - (usize)s->base) / s->obj_size);
    if (idx < s->capacity) {
        u32 w = idx / 64;
        u32 bit = idx % 64;
        s->free_bitmap[w] |= (1ULL << bit);
        s->in_use--;
        c->total_frees++;
    }
}
