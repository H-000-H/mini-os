/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file redef.h
 * @brief Host test harness stub for redef.h
 *
 * The real inc/redef.h contains Cortex-M inline assembly (mrs primask, cpsid),
 * which cannot compile on the host. This stub replaces it when building the
 * memory module for host testing: same type/macro names (mini_os_/MINI_OS_),
 * host-friendly irq no-ops and __atomic builtins. The test build injects this
 * file first via "-include redef.h", and the REDEF_H guard then blocks the
 * real inc/redef.h.
 *
 * Only the parts used by memory.h / memory.c / err.h are provided.
 */
#ifndef REDEF_H
#define REDEF_H

#include <stddef.h>

/* ------------------------------- base types ------------------------------ */
typedef signed char         mini_os_int8_t;
typedef signed short        mini_os_int16_t;
typedef signed int          mini_os_int32_t;
typedef signed long long    mini_os_int64_t;
typedef unsigned char       mini_os_uint8_t;
typedef unsigned short      mini_os_uint16_t;
typedef unsigned int        mini_os_uint32_t;
typedef unsigned long long  mini_os_uint64_t;

typedef mini_os_int8_t      mini_os_bool_t;
typedef size_t              mini_os_size_t;

/* --------------------------- volatile base types ------------------------- */
typedef volatile signed char        mini_os_volatile_int8_t;
typedef volatile signed short       mini_os_volatile_int16_t;
typedef volatile signed int         mini_os_volatile_int32_t;
typedef volatile unsigned char      mini_os_volatile_uint8_t;
typedef volatile unsigned short     mini_os_volatile_uint16_t;
typedef volatile unsigned int       mini_os_volatile_uint32_t;

/* ----------------------------- self types -------------------------------- */
typedef signed long         mini_os_irq_t;
typedef mini_os_int32_t     mini_os_err_t;

#define MINI_OS_TRUE    1
#define MINI_OS_FALSE   0
#define MINI_OS_NULL    ((void *)0)

#define MINI_OS_STATIC_INLINE static inline
#define MINI_OS_SEQ_CST __ATOMIC_SEQ_CST

/* --------------------- atomic types (host: plain + builtins) --------------- */
typedef mini_os_int32_t  mini_os_atomic_int32_t;
typedef mini_os_uint32_t mini_os_atomic_uint32_t;

/* --------------------- constructors (host: clang attribute) --------------- */
#define MINI_OS_CONSTRUCTOR(x) __attribute__((constructor(x)))

/* --------------------- static assert --------------------------------------- */
#define MINI_OS_ASSERT(condition, fmt) _Static_assert(condition, fmt)

/* --------------------------- container_of ---------------------------------- */
#define mini_os_container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

/* --------------------------- memset (freestanding) ------------------------- */
#define MINI_OS_MEMSET(dst, val, n) __builtin_memset((dst), (val), (n))

/* -------------------- host irq layer (interrupts = no-op) ----------------- */
MINI_OS_STATIC_INLINE mini_os_irq_t mini_os_irq_save(void)
{
    return 0;
}

MINI_OS_STATIC_INLINE void mini_os_irq_restore(mini_os_irq_t irq_level)
{
    (void)irq_level;
}

/* ------------------------ host atomics (clang builtins) ------------------- */
#define MINI_OS_ATOMIC_STORE(ptr, value, mem) __atomic_store_n((ptr), (value), (mem))
#define MINI_OS_ATOMIC_LOAD(ptr, mem)         __atomic_load_n((ptr), (mem))
#define MINI_OS_ATOMIC_CAS(ptr, expected, desired, mem_success, mem_fail) \
    __atomic_compare_exchange_n((ptr), (expected), (desired), 0, (mem_success), (mem_fail))

#endif /* REDEF_H */
