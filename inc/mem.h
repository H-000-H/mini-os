/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mem.h
 * @brief Static buffer pool header (no heap: caller-provided pool and memory)
 * @author H-000-H
 */

#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "redef.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include "mini_config.h"

/* Atomic counter type. Actual atomic ops are wrapped inside buffer_pool.c via BUFF_POOL_* macros. */
typedef volatile mini_uint32_t buff_pool_atomic_uint_t;

/* Maximum number of pool segments (initial + expansions) */
#define BUFF_POOL_MAX_SEGS 4u

/* One pool segment (initial pool or an appended expansion segment) */
struct buff_pool_seg
{
    mini_uint8_t* base;
    mini_size_t len; /**< segment bytes */
};

/* Opaque free-list node type (defined in buffer_pool.c) */
struct buff_pool_free_block;

/* Pool state; the instance is provided by the caller (static storage).
 * This module never allocates memory. */
struct buffer_pool
{
    char name[POOL_NAME_LEN]; /**< debug label */
    mini_uint8_t* pool_base;  /**< initial pool base (caller-provided) */
    mini_size_t pool_size;         /**< initial pool bytes */
    struct buff_pool_free_block* free_list; /**< free-list head */
    mini_size_t total_size;        /**< cumulative size of all segments */
    struct buff_pool_seg segs[BUFF_POOL_MAX_SEGS]; /**< segment table, sorted by length ascending */
    mini_uint32_t seg_count;  /**< number of registered segments */
    buff_pool_atomic_uint_t used_count; /**< currently allocated block count */
    buff_pool_atomic_uint_t peak;       /**< peak usage */
};
typedef struct buffer_pool buffer_pool_t;

/* Allocated block: dual-pointer ring read/write buffer.
 * head (write) and tail (read) are atomic variables; 'used' is a separate
 * counter so a full buffer (head == tail) is not mistaken for an empty one. */
typedef struct buffer_block
{
    struct buffer_pool* pool; /**< owning pool */
    mini_uint8_t* raw;        /**< original address inside pool */
    mini_uint8_t* data;       /**< data area start (right after raw) */
    mini_size_t capacity;          /**< data area capacity in bytes */
    buff_pool_atomic_uint_t head; /**< dual-pointer: write index */
    buff_pool_atomic_uint_t tail; /**< dual-pointer: read index */
    buff_pool_atomic_uint_t used; /**< bytes written but not yet read */
} buffer_block_t;

typedef struct buffer_pool_config
{
    const char name[POOL_NAME_LEN]; /**< debug label */
    void* static_mem;               /**< pool memory base (required) */
    mini_size_t static_len;              /**< pool memory bytes (required) */
} buffer_pool_config_t;

/* ---------------------------------------------------------------------- */
/* Lifetime                                                               */
/* ---------------------------------------------------------------------- */
/**
 * @brief Initialize a caller-provided buffer pool (free-list + dual-pointer block allocator)
 * @param[in] pool pool instance (static storage, e.g. `static buffer_pool_t pool;`)
 * @param[in] config configuration (name, static_mem, static_len); all required
 * @return 0 = success; negative errno on invalid arguments
 * @note No heap is used: both the pool struct and the pool memory are
 *       owned by the caller for the pool's entire lifetime.
 */
int buffer_pool_init(buffer_pool_t* pool, const buffer_pool_config_t* config);
/**
 * @brief De-initialize the buffer pool (resets state; owns no resources)
 * @param[in] pool pool handle (may be NULL)
 */
void buffer_pool_deinit(buffer_pool_t* pool);

/* ---------------------------------------------------------------------- */
/* Allocation / Free                                                      */
/* ---------------------------------------------------------------------- */
/**
 * @brief Allocate a block of the requested size from the static pool
 * @param[in] pool pool handle
 * @param[in] size requested data-area bytes
 * @return block handle; NULL when the static pool cannot satisfy the request
 */
buffer_block_t* buffer_pool_alloc(buffer_pool_t* pool, mini_size_t size);
/**
 * @brief Allocate a block from the static pool only (same as buffer_pool_alloc)
 * @param[in] pool pool handle
 * @param[in] size requested data-area bytes
 * @return block handle; NULL when the static pool has no space left
 */
buffer_block_t* buffer_pool_alloc_static(buffer_pool_t* pool, mini_size_t size);
/**
 * @brief Append a memory segment to the static pool (runtime expansion)
 * @param[in] pool pool handle
 * @param[in] mem segment base (caller-owned; must not overlap pool memory or any block)
 * @param[in] len segment bytes (>= header + minimum block)
 * @return 0 = success; negative errno = invalid argument / segment table full
 * @note Existing blocks are never moved; the new segment is added to the
 *       free list and adjacent free blocks are coalesced automatically.
 */
int buffer_pool_expand(buffer_pool_t* pool, void* mem, mini_size_t len);
/**
 * @brief Return a block to the pool free list (coalesces adjacent free blocks)
 * @param[in] pool pool handle
 * @param[in] block block to free (must have been allocated by this pool)
 */
void buffer_pool_free(buffer_pool_t* pool, buffer_block_t* block);

/* ISR-safe variants. The critical section masks interrupts (PRIMASK), so
 * the ISR path is identical to the thread path — no heap is ever touched. */
/**
 * @brief Allocate a block from ISR context
 * @param[in] pool pool handle
 * @param[in] size requested bytes
 * @return block handle; NULL when the static pool cannot satisfy the request
 */
buffer_block_t* buffer_pool_alloc_isr(buffer_pool_t* pool, mini_size_t size);
/**
 * @brief Free a block from ISR context
 * @param[in] pool pool handle
 * @param[in] block block to free
 */
void buffer_pool_free_isr(buffer_pool_t* pool, buffer_block_t* block);

/* ---------------------------------------------------------------------- */
/* Block content management (dual-pointer ring read/write; actual byte    */
/* counts via out-pointer, return value reports status only)              */
/* ---------------------------------------------------------------------- */
/**
 * @brief Write data into the block (head advances with wrap-around; truncated when full)
 * @param[in] block block handle
 * @param[in] src source data
 * @param[in] len requested write bytes
 * @param[out] actual optional: actual bytes written (<= len), may be NULL
 * @return 0 = success; negative errno = invalid argument
 * @note When the block is nearly full, writes are truncated to the remaining
 *       space but still return 0 (success); use *actual to detect short writes.
 */
int buffer_block_write(buffer_block_t* block, const void* src, mini_size_t len, mini_size_t* actual);
/**
 * @brief Read data from the block (tail advances with wrap-around; returns 0 bytes when empty)
 * @param[in] block block handle
 * @param[out] dst destination buffer
 * @param[in] len requested read bytes
 * @param[out] actual optional: actual bytes read (<= len), may be NULL
 * @return 0 = success; negative errno = invalid argument
 * @note When data is insufficient, reads are truncated but still return 0
 *       (success); use *actual to detect short reads.
 */
int buffer_block_read(buffer_block_t* block, void* dst, mini_size_t len, mini_size_t* actual);
/**
 * @brief Get the number of bytes written but not yet read
 * @param[in] block block handle
 * @return number of occupied bytes
 */
mini_size_t buffer_block_used(const buffer_block_t* block);
/**
 * @brief Get the remaining writable bytes in the block
 * @param[in] block block handle
 * @return number of free bytes
 */
mini_size_t buffer_block_space(const buffer_block_t* block);
/**
 * @brief Reset the dual pointers (clear block content)
 * @param[in] block block handle
 */
void buffer_block_reset(buffer_block_t* block);

/* ---------------------------------------------------------------------- */
/* Statistics / Diagnostics                                               */
/* ---------------------------------------------------------------------- */
/**
 * @brief Get the total pool size (all segments)
 * @param[in] pool pool handle
 * @return total pool bytes
 */
mini_size_t buffer_pool_size(const buffer_pool_t* pool);
/**
 * @brief Walk the free list to get currently allocatable (splittable) bytes
 * @param[in] pool pool handle
 * @return free bytes
 */
mini_size_t buffer_pool_free_space(const buffer_pool_t* pool);
/**
 * @brief Get the current number of allocated blocks
 * @param[in] pool pool handle
 * @return number of allocated blocks
 */
mini_uint32_t buffer_pool_used(const buffer_pool_t* pool);
/**
 * @brief Get the historical peak usage (for debugging / certification)
 * @param[in] pool pool handle
 * @return peak block count
 */
mini_uint32_t buffer_pool_peak(const buffer_pool_t* pool);
/**
 * @brief Reset the peak counter
 * @param[in] pool pool handle
 */
void buffer_pool_reset_peak(buffer_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_POOL_H */
