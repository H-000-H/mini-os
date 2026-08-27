/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file buffer_pool.c
 * @brief Static buffer pool implementation (segmented free-list management, no heap)
 * @author  H-000-H
 */

#include "mem.h"
#include "err.h"

/* -------------------------------------------------------------------------- */
/* Alignment granularity and minimum block                                    */
/* -------------------------------------------------------------------------- */
#define BUFF_POOL_ALIGN_SIZE 8u /* alignment granularity for blocks and data */
#define BUFF_POOL_MIN_BLOCK 16u /* do not split when the remainder is below this */

#define BUFF_POOL_ALIGN_UP(x, a) (((x) + ((a) - 1u)) & ~((a) - 1u))

/* -------------------------------------------------------------------------- */
/* Free block header (embedded in pool memory; shares the header area with    */
/* the allocated block metadata)                                              */
/* -------------------------------------------------------------------------- */
struct buff_pool_free_block
{
    struct buff_pool_free_block* prev;
    struct buff_pool_free_block* next;
    size_t size; /**< data bytes of this free block */
};

/* Unified header size: free_block when free, buffer_block_t when allocated */
#define BUFF_POOL_HEAD_SIZE (BUFF_POOL_ALIGN_UP((sizeof(struct buff_pool_free_block) > sizeof(buffer_block_t) ? sizeof(struct buff_pool_free_block) : sizeof(buffer_block_t)), BUFF_POOL_ALIGN_SIZE))

/* -------------------------------------------------------------------------- */
/* Pool critical section (lock)                                               */
/* -------------------------------------------------------------------------- */

/* Single-core bare-metal: masking interrupts is the critical section. */
static void buff_pool_lock_enter(buffer_pool_t* pool, mini_uint32_t* state)
{
    (void)pool;
    *state = (mini_uint32_t)mini_irq_save();
}

static void buff_pool_lock_exit(buffer_pool_t* pool, mini_uint32_t state)
{
    (void)pool;
    mini_irq_restore((mini_irq_t)state);
}

/* Atomic helpers built on redef.h primitives. All call sites hold the pool
 * lock (interrupts masked), so load/store ordering is sufficient. The CAS
 * is used by the lock-free block read/write paths (SPSC, no pool lock). */
#define BUFF_POOL_ATOMIC_STORE(ptr, val) MINI_ATOMIC_STORE((ptr), (val), MINI_SEQ_CST)
#define BUFF_POOL_ATOMIC_LOAD(ptr)       MINI_ATOMIC_LOAD((ptr), MINI_SEQ_CST)
#define BUFF_POOL_ATOMIC_CAS(ptr, exp, des) MINI_ATOMIC_CAS((ptr), (exp), (des), MINI_SEQ_CST, MINI_SEQ_CST)

/* -------------------------------------------------------------------------- */
/* Free-list operations (caller must hold the pool lock)                      */
/* -------------------------------------------------------------------------- */

static void buff_pool_freelist_push(struct buffer_pool* pool, struct buff_pool_free_block* blk)
{
    blk->prev = NULL;
    blk->next = pool->free_list;
    if (pool->free_list != NULL)
        pool->free_list->prev = blk;
    pool->free_list = blk;
}

static void buff_pool_freelist_remove(struct buffer_pool* pool, struct buff_pool_free_block* blk)
{
    if (blk->prev != NULL)
        blk->prev->next = blk->next;
    else
        pool->free_list = blk->next;
    if (blk->next != NULL)
        blk->next->prev = blk->prev;
    blk->prev = NULL;
    blk->next = NULL;
}

/* Return a block to the list, coalescing with address-adjacent free blocks.
 * Strategy: absorb any adjacent free block into blk (removing it from the
 * list), then insert blk once at the end. blk is never part of the list
 * during traversal, so no self-linking can occur. */
static void buff_pool_freelist_merge(struct buffer_pool* pool, struct buff_pool_free_block* blk)
{
    struct buff_pool_free_block* it = pool->free_list;
    blk->prev = NULL;
    blk->next = NULL;
    while (it != NULL)
    {
        struct buff_pool_free_block* next = it->next;
        mini_uint8_t* it_end = (mini_uint8_t*)it + BUFF_POOL_HEAD_SIZE + it->size;
        mini_uint8_t* blk_end = (mini_uint8_t*)blk + BUFF_POOL_HEAD_SIZE + blk->size;

        if (it_end == (mini_uint8_t*)blk) /* it immediately precedes blk */
        {
            it->size += BUFF_POOL_HEAD_SIZE + blk->size;
            buff_pool_freelist_remove(pool, it);
            blk = it; /* blk grows backwards to cover it; it is now off-list */
        }
        else if (blk_end == (mini_uint8_t*)it) /* blk immediately precedes it */
        {
            blk->size += BUFF_POOL_HEAD_SIZE + it->size;
            buff_pool_freelist_remove(pool, it);
        }
        it = next;
    }
    buff_pool_freelist_push(pool, blk);
}

/* First-fit: find a free block with data >= size; split and keep the remainder.
 * If seg != NULL, only free blocks located inside that segment are considered
 * (address range check). The chosen block is REMOVED from the list and handed
 * to the caller; the remainder (split) stays in the list where the block was. */
static struct buff_pool_free_block* buff_pool_freelist_alloc(struct buffer_pool* pool, size_t size, const struct buff_pool_seg* seg)
{
    struct buff_pool_free_block* it = pool->free_list;
    mini_uint8_t* s_base = seg ? seg->base : NULL;
    mini_uint8_t* s_end = seg ? seg->base + seg->len : NULL;
    size = BUFF_POOL_ALIGN_UP(size, BUFF_POOL_ALIGN_SIZE);
    while (it != NULL)
    {
        struct buff_pool_free_block* next = it->next;
        if (seg != NULL)
        {
            if ((mini_uint8_t*)it < s_base || (mini_uint8_t*)it >= s_end)
            {
                it = next; /* block lies outside this segment */
                continue;
            }
        }
        if (it->size >= size)
        {
            size_t remain = it->size - size;
            if (remain >= BUFF_POOL_HEAD_SIZE + BUFF_POOL_MIN_BLOCK)
            {
                struct buff_pool_free_block* split = (struct buff_pool_free_block*)((mini_uint8_t*)it + BUFF_POOL_HEAD_SIZE + size);
                buff_pool_freelist_remove(pool, it);
                split->prev = NULL;
                split->next = NULL;
                split->size = remain - BUFF_POOL_HEAD_SIZE;
                buff_pool_freelist_push(pool, split); /* remainder stays in the list */
                it->size = size; /* shrink the chosen block (now off-list) */
            }
            else
            {
                buff_pool_freelist_remove(pool, it);
            }
            return it;
        }
        it = next;
    }
    return NULL;
}

/* Largest single free block inside one segment (caller holds the lock) */
static size_t buff_pool_seg_max_free(const struct buffer_pool* pool, const struct buff_pool_seg* seg)
{
    const struct buff_pool_free_block* it = pool->free_list;
    mini_uint8_t* s_base = seg->base;
    mini_uint8_t* s_end = seg->base + seg->len;
    size_t maxf = 0u;
    while (it != NULL)
    {
        if ((mini_uint8_t*)it >= s_base && (mini_uint8_t*)it < s_end && it->size > maxf)
            maxf = it->size;
        it = it->next;
    }
    return maxf;
}

/* Register a segment, keeping the table sorted by length ascending.
 * Returns 0 on success, -1 when the table is full. */
static int buff_pool_seg_add(struct buffer_pool* pool, mini_uint8_t* base, size_t len)
{
    mini_uint32_t insert_index, shift_index;
    if (pool->seg_count >= BUFF_POOL_MAX_SEGS)
        return -1;
    for (insert_index = 0u; insert_index < pool->seg_count; insert_index++)
    {
        if (len < pool->segs[insert_index].len)
            break;
    }
    for (shift_index = pool->seg_count; shift_index > insert_index; shift_index--)
        pool->segs[shift_index] = pool->segs[shift_index - 1u];
    pool->segs[insert_index].base = base;
    pool->segs[insert_index].len = len;
    pool->seg_count++;
    return 0;
}

/* Cut a block from the static pool.
 * Segments are tried from the smallest upwards (table sorted by length asc):
 * the smallest segment is drained first, larger segments only when it can
 * no longer satisfy the request. Returns NULL when no segment fits. */
static buffer_block_t* buff_pool_alloc_static_impl(struct buffer_pool* pool, size_t size)
{
    mini_uint32_t seg_index;
    struct buff_pool_free_block* free_block;
    buffer_block_t* blk;
    if (pool->pool_base == NULL || pool->seg_count == 0u)
        return NULL;
    for (seg_index = 0u; seg_index < pool->seg_count; seg_index++)
    {
        struct buff_pool_seg* seg = &pool->segs[seg_index];
        if (buff_pool_seg_max_free(pool, seg) < size)
            continue; /* this segment cannot satisfy; try the next larger one */
        free_block = buff_pool_freelist_alloc(pool, size, seg);
        if (free_block == NULL)
            continue; /* fragmented; try the next segment */
        {
            size_t cap = free_block->size;
            blk = (buffer_block_t*)free_block;
            blk->raw = (mini_uint8_t*)free_block;
            blk->data = (mini_uint8_t*)free_block + BUFF_POOL_HEAD_SIZE;
            blk->capacity = cap;
            BUFF_POOL_ATOMIC_STORE(&blk->head, 0u);
            BUFF_POOL_ATOMIC_STORE(&blk->tail, 0u);
            BUFF_POOL_ATOMIC_STORE(&blk->used, 0u);
        }
        return blk;
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

int buffer_pool_init(buffer_pool_t* pool, const buffer_pool_config_t* config)
{
    size_t total;
    size_t name_len;
    struct buff_pool_free_block* whole;
    if (pool == NULL || config == NULL || config->static_mem == NULL)
        return MINI_ERR_INVAL;
    total = config->static_len;
    if (total < BUFF_POOL_HEAD_SIZE + BUFF_POOL_MIN_BLOCK)
        return MINI_ERR_INVAL;

    /* Copy the debug label (bounded, always NUL-terminated) */
    name_len = 0u;
    while (name_len < (size_t)(POOL_NAME_LEN - 1) && config->name[name_len] != '\0')
    {
        pool->name[name_len] = config->name[name_len];
        name_len++;
    }
    pool->name[name_len] = '\0';

    pool->pool_base = (mini_uint8_t*)config->static_mem;
    pool->pool_size = total;
    pool->total_size = total;
    pool->free_list = NULL;
    pool->seg_count = 0u;
    BUFF_POOL_ATOMIC_STORE(&pool->used_count, 0u);
    BUFF_POOL_ATOMIC_STORE(&pool->peak, 0u);

    /* Register the initial segment; the whole pool becomes a single free block */
    if (buff_pool_seg_add(pool, pool->pool_base, total) != 0)
        return MINI_ERR_NOSPC; /* cannot happen: the table was empty */
    whole = (struct buff_pool_free_block*)pool->pool_base;
    whole->prev = NULL;
    whole->next = NULL;
    whole->size = total - BUFF_POOL_HEAD_SIZE;
    pool->free_list = whole;
    return MINI_OK;
}

void buffer_pool_deinit(buffer_pool_t* pool)
{
    if (pool == NULL)
        return;
    pool->free_list = NULL;
    pool->seg_count = 0u;
    pool->pool_base = NULL;
    pool->pool_size = 0u;
    pool->total_size = 0u;
    BUFF_POOL_ATOMIC_STORE(&pool->used_count, 0u);
    BUFF_POOL_ATOMIC_STORE(&pool->peak, 0u);
}

/* Bookkeeping for a successful allocation (caller holds the lock) */
static void buff_pool_count_alloc(buffer_pool_t* pool, buffer_block_t* blk)
{
    mini_uint32_t used;
    blk->pool = pool;
    used = BUFF_POOL_ATOMIC_LOAD(&pool->used_count) + 1u;
    BUFF_POOL_ATOMIC_STORE(&pool->used_count, used);
    if (used > BUFF_POOL_ATOMIC_LOAD(&pool->peak))
        BUFF_POOL_ATOMIC_STORE(&pool->peak, used);
}

buffer_block_t* buffer_pool_alloc(buffer_pool_t* pool, size_t size)
{
    buffer_block_t* blk;
    mini_uint32_t lock_state;
    if (pool == NULL || size == 0u)
        return NULL;
    buff_pool_lock_enter(pool, &lock_state);
    blk = buff_pool_alloc_static_impl(pool, size);
    if (blk != NULL)
        buff_pool_count_alloc(pool, blk);
    buff_pool_lock_exit(pool, lock_state);
    return blk;
}

buffer_block_t* buffer_pool_alloc_static(buffer_pool_t* pool, size_t size)
{
    return buffer_pool_alloc(pool, size);
}

int buffer_pool_expand(buffer_pool_t* pool, void* mem, size_t len)
{
    struct buff_pool_free_block* seg;
    mini_uint32_t lock_state;
    if (pool == NULL || mem == NULL || len < BUFF_POOL_HEAD_SIZE + BUFF_POOL_MIN_BLOCK)
        return MINI_ERR_INVAL;
    buff_pool_lock_enter(pool, &lock_state);
    if (buff_pool_seg_add(pool, (mini_uint8_t*)mem, len) != 0) /* segment table full */
    {
        buff_pool_lock_exit(pool, lock_state);
        return MINI_ERR_NOSPC;
    }
    seg = (struct buff_pool_free_block*)mem;
    seg->prev = NULL;
    seg->next = NULL;
    seg->size = len - BUFF_POOL_HEAD_SIZE;
    buff_pool_freelist_merge(pool, seg); /* coalesces with adjacent free blocks */
    pool->total_size += len;
    buff_pool_lock_exit(pool, lock_state);
    return MINI_OK;
}

void buffer_pool_free(buffer_pool_t* pool, buffer_block_t* block)
{
    mini_uint32_t lock_state;
    if (pool == NULL || block == NULL)
        return;
    buff_pool_lock_enter(pool, &lock_state);
    /* At allocation time buffer_block_t.data overwrites the free_block.size
     * offset, so restore size from block->capacity before returning to the list. */
    {
        struct buff_pool_free_block* free_block = (struct buff_pool_free_block*)block->raw;
        free_block->size = block->capacity;
        buff_pool_freelist_merge(pool, free_block);
    }
    {
        mini_uint32_t used = BUFF_POOL_ATOMIC_LOAD(&pool->used_count);
        if (used > 0u)
            BUFF_POOL_ATOMIC_STORE(&pool->used_count, used - 1u);
    }
    buff_pool_lock_exit(pool, lock_state);
}

/* -------------------------------------------------------------------------- */
/* ISR-safe API                                                               */
/* The critical section masks interrupts, so the ISR path is identical to the */
/* thread path — this module never touches a heap.                            */
/* -------------------------------------------------------------------------- */

buffer_block_t* buffer_pool_alloc_isr(buffer_pool_t* pool, size_t size)
{
    return buffer_pool_alloc(pool, size);
}

void buffer_pool_free_isr(buffer_pool_t* pool, buffer_block_t* block)
{
    buffer_pool_free(pool, block);
}

/* -------------------------------------------------------------------------- */
/* Block content management (dual-pointer ring read/write; SPSC semantics,   */
/* head/tail atomic)                                                          */
/* -------------------------------------------------------------------------- */

static void buff_pool_memcpy(void* dst, const void* src, size_t n)
{
    mini_uint8_t* d = (mini_uint8_t*)dst;
    const mini_uint8_t* s = (const mini_uint8_t*)src;
    while (n-- > 0u)
        *d++ = *s++;
}

size_t buffer_block_used(const buffer_block_t* block)
{
    if (block == NULL || block->capacity == 0u)
        return 0u;
    return (size_t)BUFF_POOL_ATOMIC_LOAD(&block->used);
}

size_t buffer_block_space(const buffer_block_t* block)
{
    if (block == NULL)
        return 0u;
    return block->capacity - buffer_block_used(block);
}

int buffer_block_write(buffer_block_t* block, const void* src, size_t len, size_t* actual)
{
    const mini_uint8_t* src_bytes = (const mini_uint8_t*)src;
    size_t space;
    size_t first;
    mini_uint32_t head;
    if (block == NULL || src == NULL || len == 0u)
    {
        if (actual != NULL)
            *actual = 0u;
        return MINI_ERR_INVAL;
    }
    space = buffer_block_space(block);
    if (len > space)
        len = space;
    if (len == 0u)
    {
        if (actual != NULL)
            *actual = 0u;
        return MINI_OK; /* block full, truncated to 0, still success */
    }
    head = BUFF_POOL_ATOMIC_LOAD(&block->head);
    first = block->capacity - (size_t)head;
    if (first > len)
        first = len;
    buff_pool_memcpy(&block->data[head], src_bytes, first);
    if (len > first)
        buff_pool_memcpy(block->data, &src_bytes[first], len - first);
    BUFF_POOL_ATOMIC_STORE(&block->head, (head + (mini_uint32_t)len) % (mini_uint32_t)block->capacity);
    /* Publish the data (head) first, then the byte count. */
    {
        mini_uint32_t used = BUFF_POOL_ATOMIC_LOAD(&block->used);
        while (!BUFF_POOL_ATOMIC_CAS(&block->used, &used, used + (mini_uint32_t)len))
        {
        }
    }
    if (actual != NULL)
        *actual = len;
    return MINI_OK;
}

int buffer_block_read(buffer_block_t* block, void* dst, size_t len, size_t* actual)
{
    mini_uint8_t* dst_bytes = (mini_uint8_t*)dst;
    size_t used;
    size_t first;
    mini_uint32_t tail;
    if (block == NULL || dst == NULL || len == 0u)
    {
        if (actual != NULL)
            *actual = 0u;
        return MINI_ERR_INVAL;
    }
    used = buffer_block_used(block);
    if (len > used)
        len = used;
    if (len == 0u)
    {
        if (actual != NULL)
            *actual = 0u;
        return MINI_OK; /* block empty, truncated to 0, still success */
    }
    tail = BUFF_POOL_ATOMIC_LOAD(&block->tail);
    first = block->capacity - (size_t)tail;
    if (first > len)
        first = len;
    buff_pool_memcpy(dst_bytes, &block->data[tail], first);
    if (len > first)
        buff_pool_memcpy(&dst_bytes[first], block->data, len - first);
    BUFF_POOL_ATOMIC_STORE(&block->tail, (tail + (mini_uint32_t)len) % (mini_uint32_t)block->capacity);
    /* Publish the consumption (tail) first, then drop the byte count. */
    {
        mini_uint32_t used = BUFF_POOL_ATOMIC_LOAD(&block->used);
        while (!BUFF_POOL_ATOMIC_CAS(&block->used, &used, used - (mini_uint32_t)len))
        {
        }
    }
    if (actual != NULL)
        *actual = len;
    return MINI_OK;
}

void buffer_block_reset(buffer_block_t* block)
{
    if (block != NULL)
    {
        BUFF_POOL_ATOMIC_STORE(&block->head, 0u);
        BUFF_POOL_ATOMIC_STORE(&block->tail, 0u);
        BUFF_POOL_ATOMIC_STORE(&block->used, 0u);
    }
}

/* -------------------------------------------------------------------------- */
/* Statistics / diagnostics                                                   */
/* -------------------------------------------------------------------------- */

size_t buffer_pool_size(const buffer_pool_t* pool)
{
    if (pool == NULL)
        return 0u;
    return pool->total_size;
}

size_t buffer_pool_free_space(const buffer_pool_t* pool)
{
    const struct buff_pool_free_block* it;
    size_t total = 0u;
    mini_uint32_t lock_state;
    if (pool == NULL || pool->pool_base == NULL)
        return 0u;
    /* Walk the free list under the lock to avoid concurrent mutation */
    buff_pool_lock_enter((buffer_pool_t*)pool, &lock_state);
    for (it = pool->free_list; it != NULL; it = it->next)
        total += BUFF_POOL_HEAD_SIZE + it->size;
    buff_pool_lock_exit((buffer_pool_t*)pool, lock_state);
    return total;
}

mini_uint32_t buffer_pool_used(const buffer_pool_t* pool)
{
    if (pool == NULL)
        return 0u;
    return BUFF_POOL_ATOMIC_LOAD(&pool->used_count);
}

mini_uint32_t buffer_pool_peak(const buffer_pool_t* pool)
{
    if (pool == NULL)
        return 0u;
    return BUFF_POOL_ATOMIC_LOAD(&pool->peak);
}

void buffer_pool_reset_peak(buffer_pool_t* pool)
{
    if (pool != NULL)
        BUFF_POOL_ATOMIC_STORE(&pool->peak, BUFF_POOL_ATOMIC_LOAD(&pool->used_count));
}
