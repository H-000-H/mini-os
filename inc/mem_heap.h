/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mem_heap.h
 * @brief Memory heap definition and link lds script only control heap size
 * @author H-000-H
 */
#ifndef MEM_HEAP_H
#define MEM_HEAP_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "redef.h"
#include "mini_config.h"
extern char __mini_os_heap_start[];
extern char __mini_os_heap_end[];
#define MINI_OS_HEAP_SIZE ((mini_os_size_t)(__mini_os_heap_end - __mini_os_heap_start))
/**
 * @brief Slab page size
 * @note
 *  - Slab page size must be a power of 2 and not exceed 64KB (default 2KB)
 *  - Slab zone is carved out of the pool once at init: pages = min(MINI_OS_SLAB_PAGE_MAX, heap/MINI_OS_SLAB_PROPORTION / page size)
 *  - normal it's take up 1/4 or 1/5 of the heap and peer slab buffer in page mini default 16
 */
#ifdef CONFIG_MINI_OS_SLAB_PAGE_SIZE
#define MINI_OS_SLAB_PAGE_SIZE CONFIG_MINI_OS_SLAB_PAGE_SIZE
#else
#define MINI_OS_SLAB_PAGE_SIZE 2048
#endif
#if defined(CONFIG_MINI_OS_SLAB_PAGE_MAX_SIZE)
#define MINI_OS_SLAB_PAGE_MAX_SIZE CONFIG_MINI_OS_SLAB_PAGE_MAX_SIZE
#else
#define MINI_OS_SLAB_PAGE_MAX_SIZE (1 << 16)
#endif

#ifdef CONFIG_MINI_OS_SLAB_PAGE_MAX
#define MINI_OS_SLAB_PAGE_MAX CONFIG_MINI_OS_SLAB_PAGE_MAX
#else
#define MINI_OS_SLAB_PAGE_MAX 4
#endif

#ifdef CONFIG_MINI_OS_SLAB_PROPORTION
#define MINI_OS_SLAB_PROPORTION CONFIG_MINI_OS_SLAB_PROPORTION
#else
#define MINI_OS_SLAB_PROPORTION 4
#endif

#ifdef CONFIG_MINI_OS_SLAB_MINI_BYTES
#define MINI_OS_SLAB_MINI_BYTES CONFIG_MINI_OS_SLAB_MINI_BYTES
#else
#define MINI_OS_SLAB_MINI_BYTES 16
#endif

MINI_OS_ASSERT(MINI_OS_SLAB_MINI_BYTES>=16, "MINI_OS_SLAB_MINI_BYTES must not be less than 16");
MINI_OS_ASSERT(((MINI_OS_SLAB_PAGE_SIZE) & ((MINI_OS_SLAB_PAGE_SIZE) - 1)) == 0, "MINI_OS_SLAB_PAGE_SIZE must be a power of 2");
MINI_OS_ASSERT((MINI_OS_SLAB_PAGE_SIZE) <= (1u << 16), "MINI_OS_SLAB_PAGE_SIZE must not exceed 64KB");

/**
 * @brief check slab total take up much memory
 * @note  if slab has taken up 1/MINI_OS_SLAB_PROPORTION of the heap, return MINI_OS_ERR_NOMEM
 * @return MINI_OS_OK if slab total take up much memory, MINI_OS_ERR_NOMEM otherwise
 */
#ifdef CONFIG_OPEN_SLAB
mini_os_err_t mini_os_heap_validate(void);
#endif

#if defined(__cplusplus)
}
#endif
#endif
