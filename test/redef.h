/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file redef.h
 * @brief Host test harness stub for redef.h
 *
 * The real inc/redef.h contains Cortex-M inline assembly (mrs primask, cpsid),
 * which cannot compile on the host. This stub replaces it when building the
 * buffer pool for host testing: same type names, host-friendly irq no-ops and
 * __atomic builtins. Put this directory FIRST on the include path.
 *
 * Only the parts used by mem.h / mem.c / err.h are provided.
 */
#ifndef REDEF_H
#define REDEF_H

#include <stddef.h>

/* ------------------------------- base types ------------------------------ */
typedef signed char         mini_int8_t;
typedef signed short        mini_int16_t;
typedef signed int          mini_int32_t;
typedef signed long long    mini_int64_t;
typedef unsigned char       mini_uint8_t;
typedef unsigned short      mini_uint16_t;
typedef unsigned int        mini_uint32_t;
typedef unsigned long long  mini_uint64_t;

typedef mini_int8_t         mini_bool_t;
typedef size_t              mini_size_t;

/* --------------------------- volatile base types ------------------------- */
typedef volatile signed char        mini_volatile_int8_t;
typedef volatile signed short       mini_volatile_int16_t;
typedef volatile signed int         mini_volatile_int32_t;
typedef volatile unsigned char      mini_volatile_uint8_t;
typedef volatile unsigned short     mini_volatile_uint16_t;
typedef volatile unsigned int       mini_volatile_uint32_t;

/* ----------------------------- self types -------------------------------- */
typedef signed long         mini_irq_t;
typedef mini_int32_t        mini_err_t;

#define MINI_TRUE   1
#define MINI_FALSE  0
#define MINI_NULL   ((void *)0)

#define MINI_STATIC_INLINE static inline
#define MINI_SEQ_CST __ATOMIC_SEQ_CST

/* -------------------- host irq layer (interrupts = no-op) ----------------- */
MINI_STATIC_INLINE mini_irq_t mini_irq_save(void)
{
    return 0;
}

MINI_STATIC_INLINE void mini_irq_restore(mini_irq_t irq_level)
{
    (void)irq_level;
}

/* ------------------------ host atomics (clang builtins) ------------------- */
#define MINI_ATOMIC_STORE(ptr, value, mem) __atomic_store_n((ptr), (value), (mem))
#define MINI_ATOMIC_LOAD(ptr, mem)         __atomic_load_n((ptr), (mem))
#define MINI_ATOMIC_CAS(ptr, expected, desired, mem_success, mem_fail) \
    __atomic_compare_exchange_n((ptr), (expected), (desired), 0, (mem_success), (mem_fail))

#endif /* REDEF_H */
