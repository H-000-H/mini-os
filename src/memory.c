/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file memory.c
 * @brief malloc/free model memory manager (free list by default; with
 *        CONFIG_OPEN_SLAB small allocations go to slab; the global heap is
 *        provided by the linker script and taken over dynamically)
 * @author H-000-H
 */

#include "memory.h"
#include "mem_heap.h"
#include "err.h"
#include "redef.h"

/* -------------------------------------------------------------------------- */
/* Alignment granularity and minimum block                                    */
/* -------------------------------------------------------------------------- */
#define MINI_OS_MEMORY_ALIGN_SIZE 8u /**< data-area alignment granularity */
#define MINI_OS_MEMORY_MIN_BLOCK 16u /**< minimum free-list data block; do not split when the remainder falls below this */

#define MINI_OS_MEMORY_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

/* Free-list block header size: the header is embedded in pool memory and the
 * data area follows it, aligned to 8 bytes. */
#define MINI_OS_MEMORY_HDR_SIZE \
    MINI_OS_MEMORY_ALIGN_UP(sizeof(mini_os_buffer_freelist_config_t), MINI_OS_MEMORY_ALIGN_SIZE)

/* Header state magic: basis for double-free / invalid-pointer detection */
#define MINI_OS_MEMORY_MAGIC_ALLOC 0xA5A5A5A5u /**< block is allocated */
#define MINI_OS_MEMORY_MAGIC_FREE 0x5A5A5A5Au  /**< block is on the free list */

/**
 * @brief Insert a block at the head of the free list
 * @param[in] pool pool descriptor
 * @param[in] blk free block to insert
 * @note Also maintains the O(1) free_size accounting.
 * @note Invariant: every block on the free list has
 *       magic == MINI_OS_MEMORY_MAGIC_FREE (enforced here).
 */
static void mini_os_memory_freelist_push(struct mini_os_memory* pool, mini_os_buffer_freelist_config_t* blk)
{
    blk->magic = MINI_OS_MEMORY_MAGIC_FREE;
    pool->free_size += MINI_OS_MEMORY_HDR_SIZE + blk->size;
    mini_os_list_add(pool->free_list.next, &pool->free_list, &blk->node);
}

/**
 * @brief Coalesce address-adjacent free blocks into blk, then insert it once
 * @param[in] pool pool descriptor
 * @param[in] blk block being returned to the free list
 * @note Strategy: walk the free list, absorb every block adjacent to blk
 *       (removing it from the list), then insert blk once at the end.
 *       blk is not on the list during the traversal, so it can never
 *       self-link. Large lists make this O(n); external fragmentation is
 *       lower than merging only the immediate neighbors.
 */
static void mini_os_memory_freelist_merge(struct mini_os_memory* pool, mini_os_buffer_freelist_config_t* blk)
{
    mini_os_list_t* node = pool->free_list.next;

    while (node != &pool->free_list)
    {
        mini_os_list_t* next_node = node->next;
        mini_os_buffer_freelist_config_t* it = mini_os_container_of(node, mini_os_buffer_freelist_config_t, node);
        mini_os_uint8_t* it_end = (mini_os_uint8_t*)it + MINI_OS_MEMORY_HDR_SIZE + it->size;
        mini_os_uint8_t* blk_end = (mini_os_uint8_t*)blk + MINI_OS_MEMORY_HDR_SIZE + blk->size;

        if (it_end == (mini_os_uint8_t*)blk)
        {
            /* it immediately precedes blk: it absorbs blk backwards */
            pool->free_size -= MINI_OS_MEMORY_HDR_SIZE + it->size; /* absorbed block leaves the list */
            it->size += MINI_OS_MEMORY_HDR_SIZE + blk->size;
            mini_os_list_remove(&it->node);
            blk = it;
        }
        else if (blk_end == (mini_os_uint8_t*)it)
        {
            /* blk immediately precedes it: blk absorbs it forwards */
            pool->free_size -= MINI_OS_MEMORY_HDR_SIZE + it->size; /* absorbed block leaves the list */
            blk->size += MINI_OS_MEMORY_HDR_SIZE + it->size;
            mini_os_list_remove(&it->node);
        }
        node = next_node;
    }
    mini_os_memory_freelist_push(pool, blk);
}

/**
 * @brief First-fit allocation: find a free block whose data area >= size
 * @param[in] pool pool descriptor
 * @param[in] size requested data bytes (rounded up to MINI_OS_MEMORY_ALIGN_SIZE)
 * @return the chosen block, removed from the list and marked allocated;
 *         MINI_OS_NULL when no block fits
 * @note If the remainder is large enough it is split off and stays on the
 *       free list; the returned block's magic becomes MINI_OS_MEMORY_MAGIC_ALLOC.
 */
static mini_os_buffer_freelist_config_t* mini_os_memory_freelist_alloc(struct mini_os_memory* pool, mini_os_size_t size)
{
    mini_os_list_t* node;

    size = MINI_OS_MEMORY_ALIGN_UP(size, MINI_OS_MEMORY_ALIGN_SIZE);
    for (node = pool->free_list.next; node != &pool->free_list; node = node->next)
    {
        mini_os_buffer_freelist_config_t* it = mini_os_container_of(node, mini_os_buffer_freelist_config_t, node);

        if (it->size < size)
        {
            continue;
        }
        {
            mini_os_size_t remain = it->size - size;

            pool->free_size -= MINI_OS_MEMORY_HDR_SIZE + it->size; /* whole block leaves the list (remainder re-added by push) */
            mini_os_list_remove(&it->node);
            if (remain >= MINI_OS_MEMORY_HDR_SIZE + MINI_OS_MEMORY_MIN_BLOCK)
            {
                mini_os_buffer_freelist_config_t* split =
                    (mini_os_buffer_freelist_config_t*)((mini_os_uint8_t*)it + MINI_OS_MEMORY_HDR_SIZE + size);
                split->size = remain - MINI_OS_MEMORY_HDR_SIZE;
                mini_os_list_init(&split->node);
                mini_os_memory_freelist_push(pool, split); /* remainder stays on the list (push sets FREE) */
                it->size = size;
            }
            it->magic = MINI_OS_MEMORY_MAGIC_ALLOC; /* leaves the free list -> allocated */
            return it;
        }
    }
    return MINI_OS_NULL;
}

/**
 * @brief Return a block to the free list
 * @param[in] pool pool descriptor
 * @param[in] ptr data pointer of the block to free
 * @note The header sits before the data pointer; size/magic are preserved in
 *       the header while allocated.
 * @note Double-free detection: the magic must be MINI_OS_MEMORY_MAGIC_ALLOC,
 *       otherwise the block is already free (double free) or the pointer is
 *       invalid and the free is rejected.
 */
static void mini_os_memory_freelist_free(struct mini_os_memory* pool, void* ptr)
{
    mini_os_buffer_freelist_config_t* blk =
        (mini_os_buffer_freelist_config_t*)((mini_os_uint8_t*)ptr - MINI_OS_MEMORY_HDR_SIZE);

    if (blk->magic != MINI_OS_MEMORY_MAGIC_ALLOC)
    {
        return; /* double free or invalid pointer: header is not in the allocated state, reject */
    }
    if ((blk->size & (MINI_OS_MEMORY_ALIGN_SIZE - 1)) != 0)
    {
        return; /* corrupted header: size not aligned */
    }
    blk->magic = MINI_OS_MEMORY_MAGIC_FREE;
    mini_os_list_init(&blk->node);
    mini_os_memory_freelist_merge(pool, blk);
}

/* -------------------------------------------------------------------------- */
/* Segment table                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Register a segment, keeping the table sorted by length ascending
 * @param[in] pool pool descriptor
 * @param[in] base segment base
 * @param[in] len segment bytes
 * @return 0 on success; -1 when the table is full
 */
static mini_os_int32_t mini_os_memory_seg_add(struct mini_os_memory* pool, mini_os_uint8_t* base, mini_os_size_t len)
{
    mini_os_uint32_t insert_index;
    mini_os_uint32_t shift_index;

    if (pool->seg_count >= MINI_OS_MEMORY_MAX_SEGS)
    {
        return -1;
    }
    for (insert_index = 0; insert_index < pool->seg_count; insert_index++)
    {
        if (len < pool->segs[insert_index].len)
        {
            break;
        }
    }
    for (shift_index = pool->seg_count; shift_index > insert_index; shift_index--)
    {
        pool->segs[shift_index] = pool->segs[shift_index - 1];
    }
    pool->segs[insert_index].base = base;
    pool->segs[insert_index].len = len;
    pool->seg_count++;
    return 0;
}

/**
 * @brief Check whether a pointer lies within any registered segment
 * @param[in] pool pool descriptor
 * @param[in] ptr pointer to check
 * @return MINI_OS_TRUE when ptr falls inside a segment (with room for a header
 *         before it); MINI_OS_FALSE otherwise
 */
static mini_os_bool_t mini_os_memory_ptr_in_segments(const struct mini_os_memory* pool, const void* ptr)
{
    mini_os_uint32_t i;

    for (i = 0; i < pool->seg_count; i++)
    {
        const mini_os_uint8_t* base = pool->segs[i].base;
        const mini_os_uint8_t* end = base + pool->segs[i].len;

        if ((const mini_os_uint8_t*)ptr >= base + MINI_OS_MEMORY_HDR_SIZE && (const mini_os_uint8_t*)ptr <= end)
        {
            return MINI_OS_TRUE;
        }
    }
    return MINI_OS_FALSE;
}

/* -------------------------------------------------------------------------- */
/* Slab small-object allocation (CONFIG_OPEN_SLAB)                             */
/* A fixed zone is carved once from the pool head at init and never returned;  */
/* the zone is cut into fixed MINI_OS_SLAB_MINI_BYTES slots, free slots embed  */
/* a next pointer. When the slots are exhausted slab_alloc returns             */
/* MINI_OS_NULL and the caller falls back to the free list.                    */
/* -------------------------------------------------------------------------- */
#ifdef CONFIG_OPEN_SLAB

/**
 * @brief Carve the slab zone once at init and build the free slot list
 * @param[in] pool pool descriptor
 * @param[in] base pool memory base
 * @param[in] len pool memory bytes
 * @return MINI_OS_OK on success; MINI_OS_ERR_NOMEM when the pool is too small
 * @note Page count = min(MINI_OS_SLAB_PAGE_MAX, len/PROPORTION / PAGE_SIZE).
 *       When len < PAGE_SIZE * PROPORTION (8 KB with the default config) not
 *       even 1 page fits and initialization fails.
 */
static mini_os_err_t mini_os_memory_slab_zone_setup(struct mini_os_memory* pool, mini_os_uint8_t* base, mini_os_size_t len)
{
    mini_os_size_t pages = MINI_OS_SLAB_PAGE_MAX;
    mini_os_size_t slot_total;
    mini_os_size_t i;

    if (pages * MINI_OS_SLAB_PAGE_SIZE > len / MINI_OS_SLAB_PROPORTION)
    {
        pages = (len / MINI_OS_SLAB_PROPORTION) / MINI_OS_SLAB_PAGE_SIZE;
    }
    mini_os_single_list_init(&pool->slab_free);
    if (pages == 0)
    {
        return MINI_OS_ERR_NOMEM; /* pool smaller than page size * proportion; slab configured but 0 pages fit */
    }
    pool->slab_base = base;
    pool->slab_size = pages * MINI_OS_SLAB_PAGE_SIZE;
    pool->slab_page_count = pages;

    /* No page headers: the whole zone is cut into MINI_OS_SLAB_MINI_BYTES slots, all linked as free */
    slot_total = pool->slab_size / MINI_OS_SLAB_MINI_BYTES;
    for (i = 0; i < slot_total; i++)
    {
        mini_os_single_list_t* slot = (mini_os_single_list_t*)(base + i * MINI_OS_SLAB_MINI_BYTES);

        mini_os_single_list_init(slot);
        mini_os_single_list_push_heap(&pool->slab_free, slot);
    }
    return MINI_OS_OK;
}

/**
 * @brief Take one object slot from the slab free list
 * @param[in] pool pool descriptor
 * @return slot pointer on success; MINI_OS_NULL when the free slot list is
 *         empty (caller falls back to the free list)
 */
static void* mini_os_memory_slab_alloc(struct mini_os_memory* pool)
{
    mini_os_single_list_t* slot;

    if (mini_os_single_list_is_empty(&pool->slab_free) == MINI_OS_TRUE)
    {
        return MINI_OS_NULL; /* fixed-zone slots exhausted, no more pages carved */
    }
    slot = pool->slab_free.next;
    mini_os_single_list_remove(slot, &pool->slab_free);
    /* Occupied: the next-pointer area yields to user data (the pointer is dropped) */
    return (void*)slot;
}

/**
 * @brief Return an object slot to the slab free list
 * @param[in] pool pool descriptor
 * @param[in] ptr pointer to free
 * @return 1 = freed; 0 = pointer not inside the slab zone; -1 = inside the
 *         zone but not aligned to a slot boundary (rejected)
 */
static mini_os_int32_t mini_os_memory_slab_free(struct mini_os_memory* pool, void* ptr)
{
    const mini_os_uint8_t* p = (const mini_os_uint8_t*)ptr;

    if (pool->slab_size == 0 || p < pool->slab_base || p >= pool->slab_base + pool->slab_size)
    {
        return 0;
    }
    if (((mini_os_size_t)(p - pool->slab_base) % MINI_OS_SLAB_MINI_BYTES) != 0)
    {
        return -1; /* not aligned to a slot boundary: invalid pointer */
    }
    {
        mini_os_single_list_t* slot = (mini_os_single_list_t*)ptr;

        mini_os_single_list_init(slot);
        mini_os_single_list_push_heap(&pool->slab_free, slot);
    }
    return 1;
}

#endif /* CONFIG_OPEN_SLAB */

/* -------------------------------------------------------------------------- */
/* Pool API                                                                   */
/* -------------------------------------------------------------------------- */

mini_os_err_t mini_os_memory_init(mini_os_memory_t* pool, const mini_os_memory_config_t* config)
{
    mini_os_buffer_freelist_config_t* whole;
    mini_os_size_t name_len;

    if (pool == MINI_OS_NULL || config == MINI_OS_NULL || config->static_mem == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (config->static_len < MINI_OS_MEMORY_HDR_SIZE + MINI_OS_MEMORY_MIN_BLOCK)
    {
        return MINI_OS_ERR_INVAL;
    }

    /* Copy the debug name (bounded, always NUL-terminated) */
    name_len = 0;
    if (config->name != MINI_OS_NULL)
    {
        while (name_len < (mini_os_size_t)(MINI_OS_MEMORY_NAME_LEN - 1) && config->name[name_len] != '\0')
        {
            pool->name[name_len] = config->name[name_len];
            name_len++;
        }
    }
    pool->name[name_len] = '\0';

    pool->pool_base = (mini_os_uint8_t*)config->static_mem;
    pool->pool_size = config->static_len;
    pool->total_size = config->static_len;
    pool->free_size = 0;
    mini_os_list_init(&pool->free_list);
    pool->seg_count = 0;
    MINI_OS_ATOMIC_STORE(&pool->used_count, 0, MINI_OS_SEQ_CST);
    MINI_OS_ATOMIC_STORE(&pool->peak, 0, MINI_OS_SEQ_CST);
#ifdef CONFIG_OPEN_SLAB
    mini_os_single_list_init(&pool->slab_free);
    pool->slab_base = MINI_OS_NULL;
    pool->slab_size = 0;
    pool->slab_page_count = 0;
#endif

    /* Slab zone: carved once from the pool head at init, never returned;
     * the remainder goes to the free list */
    {
        mini_os_uint8_t* free_base = pool->pool_base;
        mini_os_size_t free_len = config->static_len;

#ifdef CONFIG_OPEN_SLAB
        if (mini_os_memory_slab_zone_setup(pool, free_base, free_len) != MINI_OS_OK)
        {
            return MINI_OS_ERR_NOMEM; /* pool too small to carve 1 slab page (threshold = page size * proportion) */
        }
        free_base += pool->slab_size;
        free_len -= pool->slab_size;
#endif
        /* Register the initial segment; the whole segment becomes a single free block */
        if (mini_os_memory_seg_add(pool, free_base, free_len) != 0)
        {
            return MINI_OS_ERR_NOSPC; /* cannot happen: the table was empty */
        }
        whole = (mini_os_buffer_freelist_config_t*)free_base;
        whole->size = free_len - MINI_OS_MEMORY_HDR_SIZE;
        mini_os_list_init(&whole->node);
        mini_os_memory_freelist_push(pool, whole);
    }
    return MINI_OS_OK;
}

mini_os_err_t mini_os_memory_deinit(mini_os_memory_t* pool)
{
    if (pool == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    mini_os_list_init(&pool->free_list);
    pool->seg_count = 0;
    pool->pool_base = MINI_OS_NULL;
    pool->pool_size = 0;
    pool->total_size = 0;
    pool->free_size = 0;
    MINI_OS_ATOMIC_STORE(&pool->used_count, 0, MINI_OS_SEQ_CST);
    MINI_OS_ATOMIC_STORE(&pool->peak, 0, MINI_OS_SEQ_CST);
#ifdef CONFIG_OPEN_SLAB
    mini_os_single_list_init(&pool->slab_free);
    pool->slab_base = MINI_OS_NULL;
    pool->slab_size = 0;
    pool->slab_page_count = 0;
#endif
    return MINI_OS_OK;
}

/**
 * @brief Bookkeeping for a successful allocation
 * @param[in] pool pool descriptor
 * @param[in] blk the newly allocated block
 * @note The caller must hold the pool lock (interrupts masked).
 */
static void mini_os_memory_count_alloc(struct mini_os_memory* pool)
{
    mini_os_uint32_t used = MINI_OS_ATOMIC_LOAD(&pool->used_count, MINI_OS_SEQ_CST) + 1;

    MINI_OS_ATOMIC_STORE(&pool->used_count, used, MINI_OS_SEQ_CST);
    if (used > MINI_OS_ATOMIC_LOAD(&pool->peak, MINI_OS_SEQ_CST))
    {
        MINI_OS_ATOMIC_STORE(&pool->peak, used, MINI_OS_SEQ_CST);
    }
}

void* mini_os_memory_alloc(mini_os_memory_t* pool, mini_os_size_t size)
{
    void* ptr = MINI_OS_NULL;
    mini_os_irq_t lock_state;

    if (pool == MINI_OS_NULL || size == 0)
    {
        return MINI_OS_NULL;
    }
    lock_state = mini_os_irq_save();
#ifdef CONFIG_OPEN_SLAB
    /* Small allocations go to slab first; fall back to the free list below when exhausted */
    if (size <= MINI_OS_SLAB_MINI_BYTES)
    {
        ptr = mini_os_memory_slab_alloc(pool);
    }
#endif
    if (ptr == MINI_OS_NULL)
    {
        mini_os_buffer_freelist_config_t* blk = mini_os_memory_freelist_alloc(pool, size);

        if (blk != MINI_OS_NULL)
        {
            ptr = (mini_os_uint8_t*)blk + MINI_OS_MEMORY_HDR_SIZE;
        }
    }
    if (ptr != MINI_OS_NULL)
    {
        mini_os_memory_count_alloc(pool);
    }
    mini_os_irq_restore(lock_state);
    return ptr;
}

mini_os_err_t mini_os_memory_free(mini_os_memory_t* pool, void* ptr)
{
    mini_os_irq_t lock_state;
    mini_os_int32_t slab_ret = 0;
    mini_os_bool_t freed = MINI_OS_FALSE;

    if (pool == MINI_OS_NULL || ptr == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    lock_state = mini_os_irq_save();
#ifdef CONFIG_OPEN_SLAB
    slab_ret = mini_os_memory_slab_free(pool, ptr);
    if (slab_ret > 0)
    {
        freed = MINI_OS_TRUE;
    }
#endif
    /* slab_ret == 0: not part of any slab page, try the free list; == -1: inside a page but invalid, reject */
    if (freed == MINI_OS_FALSE && slab_ret == 0 && mini_os_memory_ptr_in_segments(pool, ptr) == MINI_OS_TRUE)
    {
        mini_os_memory_freelist_free(pool, ptr);
        freed = MINI_OS_TRUE;
    }
    if (freed == MINI_OS_TRUE)
    {
        mini_os_uint32_t used = MINI_OS_ATOMIC_LOAD(&pool->used_count, MINI_OS_SEQ_CST);

        if (used > 0)
        {
            MINI_OS_ATOMIC_STORE(&pool->used_count, used - 1, MINI_OS_SEQ_CST);
        }
    }
    mini_os_irq_restore(lock_state);
    return (freed == MINI_OS_TRUE) ? MINI_OS_OK : MINI_OS_ERR_INVAL;
}

mini_os_err_t mini_os_memory_expand(mini_os_memory_t* pool, void* mem, mini_os_size_t len)
{
    mini_os_buffer_freelist_config_t* seg;
    mini_os_irq_t lock_state;

    if (pool == MINI_OS_NULL || mem == MINI_OS_NULL || len < MINI_OS_MEMORY_HDR_SIZE + MINI_OS_MEMORY_MIN_BLOCK)
    {
        return MINI_OS_ERR_INVAL;
    }
    lock_state = mini_os_irq_save();
    if (mini_os_memory_seg_add(pool, (mini_os_uint8_t*)mem, len) != 0)
    {
        mini_os_irq_restore(lock_state);
        return MINI_OS_ERR_NOSPC; /* segment table full */
    }
    seg = (mini_os_buffer_freelist_config_t*)mem;
    seg->size = len - MINI_OS_MEMORY_HDR_SIZE;
    mini_os_list_init(&seg->node);
    mini_os_memory_freelist_merge(pool, seg); /* coalesce with adjacent free blocks */
    pool->total_size += len;
    mini_os_irq_restore(lock_state);
    return MINI_OS_OK;
}

/* ISR-safe variants: the critical section masks interrupts, the native heap
 * is never touched; the path is identical to the thread context */
void* mini_os_memory_alloc_isr(mini_os_memory_t* pool, mini_os_size_t size)
{
    return mini_os_memory_alloc(pool, size);
}

mini_os_err_t mini_os_memory_free_isr(mini_os_memory_t* pool, void* ptr)
{
    return mini_os_memory_free(pool, ptr);
}

/* -------------------------------------------------------------------------- */
/* Statistics / diagnostics                                                   */
/* -------------------------------------------------------------------------- */

mini_os_size_t mini_os_memory_size(const mini_os_memory_t* pool)
{
    if (pool == MINI_OS_NULL)
    {
        return 0;
    }
    return pool->total_size;
}

mini_os_size_t mini_os_memory_free_space(const mini_os_memory_t* pool)
{
    mini_os_size_t total;
    mini_os_irq_t lock_state;

    if (pool == MINI_OS_NULL || pool->pool_base == MINI_OS_NULL)
    {
        return 0;
    }
    /* free_size is maintained by the alloc/free paths; no free-list walk needed */
    lock_state = mini_os_irq_save();
    total = pool->free_size;
    mini_os_irq_restore(lock_state);
    return total;
}

mini_os_uint32_t mini_os_memory_used(const mini_os_memory_t* pool)
{
    if (pool == MINI_OS_NULL)
    {
        return 0;
    }
    return MINI_OS_ATOMIC_LOAD(&pool->used_count, MINI_OS_SEQ_CST);
}

mini_os_uint32_t mini_os_memory_peak(const mini_os_memory_t* pool)
{
    if (pool == MINI_OS_NULL)
    {
        return 0;
    }
    return MINI_OS_ATOMIC_LOAD(&pool->peak, MINI_OS_SEQ_CST);
}

mini_os_err_t mini_os_memory_reset_peak(mini_os_memory_t* pool)
{
    if (pool == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    MINI_OS_ATOMIC_STORE(&pool->peak, MINI_OS_ATOMIC_LOAD(&pool->used_count, MINI_OS_SEQ_CST), MINI_OS_SEQ_CST);
    return MINI_OS_OK;
}

/* -------------------------------------------------------------------------- */
/* Global heap: dynamically takes over the linker-script heap zone             */
/* (mem_heap.h / mini-os-heap.ld)                                              */
/* -------------------------------------------------------------------------- */

static mini_os_memory_t s_mini_os_heap_pool; /**< global heap pool descriptor */
static mini_os_bool_t s_mini_os_heap_ready = MINI_OS_FALSE; /**< heap ready flag */

/**
 * @brief Validate the slab occupation ratio (only meaningful with CONFIG_OPEN_SLAB;
 *        invoked by the startup constructor; exceeding the limit is a configuration error)
 * @return MINI_OS_OK configuration valid; MINI_OS_ERR_NOMEM the slab ceiling
 *         exceeds 1/MINI_OS_SLAB_PROPORTION of the heap
 */
#ifdef CONFIG_OPEN_SLAB
MINI_OS_CONSTRUCTOR(101) mini_os_err_t mini_os_heap_validate(void)
{
    if (((mini_os_size_t)MINI_OS_SLAB_PAGE_MAX * (mini_os_size_t)MINI_OS_SLAB_PAGE_SIZE) > ((mini_os_size_t)MINI_OS_HEAP_SIZE / MINI_OS_SLAB_PROPORTION))
    {
        return MINI_OS_ERR_NOMEM;
    }
    return MINI_OS_OK;
}

/**
 * @brief Halt on a slab configuration failure (constructor, runs before main)
 */
MINI_OS_CONSTRUCTOR(101) static void mini_os_slab_validate_ctor(void)
{
    if (mini_os_heap_validate() != MINI_OS_OK)
    {
        for (;;)
        {
        }
    }
}
#endif /* CONFIG_OPEN_SLAB */

/* Heap auto-init constructor priority (value 102 wrapped in a named macro), runs before main */
#define MINI_OS_MEMORY_PRESTRUCTOR 102

/**
 * @brief Heap initialization (auto-run constructor before main): merges the
 *        linker-script heap zone into the free list, i.e. the global-heap
 *        variant of mini_os_memory_init
 */
MINI_OS_CONSTRUCTOR(MINI_OS_MEMORY_PRESTRUCTOR) static void mini_os_heap_init_ctor(void)
{
    mini_os_memory_config_t cfg;

    if ((__mini_os_heap_end - __mini_os_heap_start) <= 0)
    {
        return; /* linker script anomaly / host environment: heap stays not ready */
    }
    cfg.name = "heap";
    cfg.static_mem = (void*)__mini_os_heap_start;
    cfg.static_len = MINI_OS_HEAP_SIZE;
    if (mini_os_memory_init(&s_mini_os_heap_pool, &cfg) == MINI_OS_OK)
    {
        s_mini_os_heap_ready = MINI_OS_TRUE;
    }
}

void* mini_os_malloc(mini_os_size_t size)
{
    if (s_mini_os_heap_ready != MINI_OS_TRUE)
    {
        return MINI_OS_NULL;
    }
    return mini_os_memory_alloc(&s_mini_os_heap_pool, size);
}

mini_os_err_t mini_os_free(void* ptr)
{
    if (s_mini_os_heap_ready != MINI_OS_TRUE)
    {
        return MINI_OS_ERR_DEFER; /* heap not ready, retry later */
    }
    return mini_os_memory_free(&s_mini_os_heap_pool, ptr);
}

void* mini_os_calloc(mini_os_size_t count, mini_os_size_t size)
{
    mini_os_size_t total;
    void* ptr;

    if (count == 0 || size == 0)
    {
        return MINI_OS_NULL;
    }
    /* Multiplication overflow guard */
    if (count > ((mini_os_size_t)-1) / size)
    {
        return MINI_OS_NULL;
    }
    total = count * size;
    ptr = mini_os_malloc(total);
    if (ptr != MINI_OS_NULL)
    {
        MINI_OS_MEMSET(ptr, 0, total);
    }
    return ptr;
}
