/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file redef.h
 * @author H-OOO-H
 * @brief mini-os redefinition macros file (all symbols prefixed mini_os_/MINI_OS_)
 * @details
 *   - defines 'atomic' 'base types' 'gcc/clang differences' 'built-in macros'
 *   - this project defaults to not including standard c library headers and versions v0.1.0 defaults to 32-bit mode
 *   - temporary not included 64-bit equipment
 */
#ifndef REDEF_H
#define REDEF_H

#include <stddef.h> /* size_t, NULL (compiler-provided freestanding header, no libc) */

#if defined (__cplusplus)
extern "C" {
#endif
#include "mini_config.h"

/*---------------------------------------------------------------------------------------------------------*/
/*                                          base type                                                      */
/*---------------------------------------------------------------------------------------------------------*/
typedef signed char                         mini_os_int8_t;               /**<mini-os int8_t*/
typedef signed short                        mini_os_int16_t;              /**<mini-os int16_t*/
typedef signed int                          mini_os_int32_t;              /**<mini-os int32_t*/
typedef signed long long                    mini_os_int64_t;              /**<mini-os int64_t*/
typedef unsigned char                       mini_os_uint8_t;              /**<mini-os uint8_t*/
typedef unsigned short                      mini_os_uint16_t;             /**<mini-os uint16_t*/
typedef unsigned int                        mini_os_uint32_t;             /**<mini-os uint32_t*/
typedef unsigned long long                  mini_os_uint64_t;             /**<mini-os uint64_t*/
typedef mini_os_int8_t                      mini_os_bool_t;               /**<mini-os bool_t*/
typedef size_t                              mini_os_size_t;               /**<mini-os size_t*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                    volatile Modification base type                                      */
/*---------------------------------------------------------------------------------------------------------*/
typedef volatile signed char                mini_os_volatile_int8_t;      /**<mini-os volatile int8_t*/
typedef volatile signed short               mini_os_volatile_int16_t;     /**<mini-os volatile int16_t*/
typedef volatile signed int                 mini_os_volatile_int32_t;     /**<mini-os volatile int32_t*/
typedef volatile unsigned char              mini_os_volatile_uint8_t;     /**<mini-os volatile uint8_t*/
typedef volatile unsigned short             mini_os_volatile_uint16_t;    /**<mini-os volatile uint16_t*/
typedef volatile unsigned int               mini_os_volatile_uint32_t;    /**<mini-os volatile uint32_t*/

/*---------------------------------------------------------------------------------------------------------*/
/*                                          mini-os-self-type                                              */
/*---------------------------------------------------------------------------------------------------------*/
typedef mini_os_int32_t                     mini_os_err_t;                /**<mini-os err_t*/
typedef signed long                         mini_os_irq_t;                /**<mini-os irq_t*/
typedef signed long                         mini_os_tick_t;               /**<mini-os tick_t*/
typedef signed long                         mini_os_user_data_t;          /**<mini-os user_data_t*/

#define MINI_OS_TRUE                        (1)                         /**<true*/
#define MINI_OS_FALSE                       (0)                         /**<false*/

#ifdef MINI_OS_NULL_TO_STANDARD
#define MINI_OS_NULL                        ((void *)0)                 /**<null*/
#else
#define MINI_OS_NULL                        (0)                         /**<null*/
#endif

#define MINI_OS_UINT8_MAX                   (0XFF)                       /**<uint8_t max*/
#define MINI_OS_UINT16_MAX                  (0XFFFF)                     /**<uint16_t max*/
#define MINI_OS_UINT32_MAX                  (0xFFFFFFFF)                 /**<uint32_t max*/

/*---------------------------------------------------------------------------------------------------------*/
/*                                  mini-os-gcc-features-macro                                             */
/*---------------------------------------------------------------------------------------------------------*/
#if defined(__clang__) || defined(__GNUC__)
#define MINI_OS_VA_START(x,l)                 __builtin_va_start(x,l)   /**<va_start*/
#define MINI_OS_VA_END(x)                     __builtin_va_end(x)       /**<va_end*/
#define MINI_OS_VA_ARG(x,l)                   __builtin_va_arg(x,l)     /**<va_arg*/
#define MINI_OS_SECTION(x)                    __attribute__((section(x))) /**<section*/
#define MINI_OS_CLZ(x)                        (__builtin_clz(x))        /**<count leading zeros*/
#define MINI_OS_CTZ(x)                        (__builtin_ctz(x))        /**<count trailing zeros*/
#define MINI_OS_POPCOUNT(x)                   (__builtin_popcount(x))   /**<count set bits*/
#define MINI_OS_BSWAP(x)                      (__builtin_bswap(x))      /**<byte swap reversal big endian and little endian*/
#define MINI_OS_ALIGN(x)                      __attribute__((aligned(x))) /**<Compile-time alignment */
#define MINI_OS_UNUSED(x)                     __attribute__((unused))   /**<unused*/
#define MINI_OS_USED(x)                       __attribute__((used))     /**<used*/
#define MINI_OS_NO_RETURN                     __attribute__((noreturn)) /**<no return*/
#define MINI_OS_FORCE_INLINE                  __attribute__((always_inline))/**<Force inline*/
#define MINI_OS_WEAK                          __attribute__((weak))     /**<weak*/
#define MINI_OS_INLINE                        __inline                  /**<inline*/
#define MINI_OS_STATIC                        static                    /**<static*/
#define MINI_OS_STATIC_INLINE                 static __inline           /**<static inline*/
#define MINI_OS_STATIC_FORCE_INLINE           static __attribute__((always_inline))       /**<static force inline*/
#define MINI_OS_TYPEOF(x)                     __typeof__(x)             /**<typeof*/
#define MINI_OS_CONSTRUCTOR(x)                __attribute__((constructor(x)))               /**<constructor*/
#else
#define MINI_OS_INLINE                        inline                    /**<inline*/
#define MINI_OS_STATIC                        static                    /**<static*/
#define MINI_OS_STATIC_INLINE                 static inline             /**<static inline*/
#define MINI_OS_CONSTRUCTOR(x)                                                    /**<constructor: no standard C equivalent, no-op*/
#endif


/*---------------------------------------------------------------------------------------------------------*/
/*                                  mini-os-interrupt-functions                                            */
/*---------------------------------------------------------------------------------------------------------*/
MINI_OS_STATIC_INLINE void mini_os_irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

/**
 * @brief Enable interrupts
 */
MINI_OS_STATIC_INLINE void mini_os_irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

/**
 * @brief Save interrupt state
 */
MINI_OS_STATIC_INLINE mini_os_irq_t mini_os_irq_save(void)
{
    mini_os_irq_t irq_level;
    __asm volatile ("mrs %0, primask\n" "cpsid i\n" : "=r" (irq_level) :: "memory");
    return irq_level;
}

/**
 * @brief Restore interrupt state
 */
MINI_OS_STATIC_INLINE void mini_os_irq_restore(mini_os_irq_t irq_level)
{
    __asm volatile ("msr primask, %0\n" :: "r" (irq_level) : "memory");
}

/*---------------------------------------------------------------------------------------------------------*/
/*                                  mini-os-atomic                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#if defined (__clang__) || defined (__GNUC__)
typedef mini_os_int8_t  mini_os_atomic_int8_t;            /**<mini-os atomic int8_t*/
typedef mini_os_uint8_t mini_os_atomic_uint8_t;           /**<mini-os atomic uint8_t*/

typedef mini_os_int16_t mini_os_atomic_int16_t;           /**<mini-os atomic int16_t*/
typedef mini_os_uint16_t mini_os_atomic_uint16_t;         /**<mini-os atomic uint16_t*/

typedef mini_os_int32_t mini_os_atomic_int32_t;           /**<mini-os atomic int32_t*/
typedef mini_os_uint32_t mini_os_atomic_uint32_t;         /**<mini-os atomic uint32_t*/

#define MINI_OS_RELAXED __ATOMIC_RELAXED                  /**<mini-os atomic relaxed*/
#define MINI_OS_ACQUIRE __ATOMIC_ACQUIRE                  /**<mini-os atomic acquire*/
#define MINI_OS_RELEASE __ATOMIC_RELEASE                  /**<mini-os atomic release*/
#define MINI_OS_ACQ_REL __ATOMIC_ACQ_REL                  /**<mini-os atomic acquire-release*/
#define MINI_OS_SEQ_CST __ATOMIC_SEQ_CST                  /**<mini-os atomic sequence consistent*/
#define MINI_OS_ATOMIC_STORE(ptr, value, mem) __atomic_store_n((ptr), (value), (mem))               /**<mini-os atomic store*/
#define MINI_OS_ATOMIC_LOAD(ptr, mem) __atomic_load_n((ptr), (mem))                                 /**<mini-os atomic load*/
#define MINI_OS_ATOMIC_ADD_FETCH(ptr, value, mem) __atomic_add_fetch((ptr), (value), (mem))         /**<mini-os atomic add fetch*/
#define MINI_OS_ATOMIC_SUB_FETCH(ptr, value, mem) __atomic_sub_fetch((ptr), (value), (mem))         /**<mini-os atomic sub fetch*/
#define MINI_OS_ATOMIC_FETCH_ADD(ptr, value, mem) __atomic_fetch_add((ptr), (value), (mem))         /**<mini-os atomic fetch add*/
#define MINI_OS_ATOMIC_FETCH_SUB(ptr, value, mem) __atomic_fetch_sub((ptr), (value), (mem))         /**<mini-os atomic fetch sub*/
#define MINI_OS_ATOMIC_CAS(ptr, expected, desired, mem_success, mem_fail)\
        __atomic_compare_exchange_n((ptr), (expected), (desired),MINI_OS_TRUE, (mem_success), (mem_fail))       /**<mini-os atomic compare exchange*/
#define MINI_OS_ATOMIC_EXCHANGE(ptr, value, mem) __atomic_exchange_n((ptr), (value), (mem))         /**<mini-os atomic exchange*/
#define MINI_OS_ATOMIC_INIT(ptr,val) MINI_OS_ATOMIC_STORE((ptr), (val), MINI_OS_RELAXED)            /**<mini-os atomic init*/
#define MINI_OS_ASSERT(condition, fmt)            _Static_assert(condition, fmt)                    /**<mini-os assert*/
#else
typedef mini_os_volatile_int8_t  mini_os_atomic_int8_t;            /**<mini-os atomic int8_t*/
typedef mini_os_volatile_uint8_t mini_os_atomic_uint8_t;           /**<mini-os atomic uint8_t*/

typedef mini_os_volatile_int16_t mini_os_atomic_int16_t;           /**<mini-os atomic int16_t*/
typedef mini_os_volatile_uint16_t mini_os_atomic_uint16_t;         /**<mini-os atomic uint16_t*/

typedef mini_os_volatile_int32_t mini_os_atomic_int32_t;           /**<mini-os atomic int32_t*/
typedef mini_os_volatile_uint32_t mini_os_atomic_uint32_t;         /**<mini-os atomic uint32_t*/

#define MINI_OS_RELAXED 0
#define MINI_OS_ACQUIRE 0
#define MINI_OS_RELEASE 0
#define MINI_OS_ACQ_REL 0
#define MINI_OS_SEQ_CST 0

/*---------------------------------------------------------------------------------------------------------*/
/*                          pure ISO C11 atomic fallback (no compiler builtins)                            */
/*---------------------------------------------------------------------------------------------------------*/
/* irq save/restore makes each operation atomic against interrupts. The value-returning operations cannot be
 * written as pure-C expressions, so each is a static inline helper selected with the ISO C11 _Generic
 * keyword. The 'mem' arguments are accepted for API parity but cannot be honored without builtins. */

#define MINI_OS_ATOMIC_STORE_FN(SUFFIX, TYPE, VTYPE) \
    MINI_OS_STATIC_INLINE void mini_os_atomic_store_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_os_irq_t irq = mini_os_irq_save(); \
        *ptr = value; \
        mini_os_irq_restore(irq); \
    }

#define MINI_OS_ATOMIC_LOAD_FN(SUFFIX, TYPE, VTYPE) \
    MINI_OS_STATIC_INLINE TYPE mini_os_atomic_load_##SUFFIX(const VTYPE *ptr) \
    { \
        mini_os_irq_t irq = mini_os_irq_save(); \
        TYPE value = *ptr; \
        mini_os_irq_restore(irq); \
        return value; \
    }

#define MINI_OS_ATOMIC_FETCH_ADD_FN(SUFFIX, TYPE, VTYPE) \
    MINI_OS_STATIC_INLINE TYPE mini_os_atomic_fetch_add_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_os_irq_t irq = mini_os_irq_save(); \
        TYPE old = *ptr; \
        *ptr = old + value; \
        mini_os_irq_restore(irq); \
        return old; \
    }

#define MINI_OS_ATOMIC_FETCH_SUB_FN(SUFFIX, TYPE, VTYPE) \
    MINI_OS_STATIC_INLINE TYPE mini_os_atomic_fetch_sub_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_os_irq_t irq = mini_os_irq_save(); \
        TYPE old = *ptr; \
        *ptr = old - value; \
        mini_os_irq_restore(irq); \
        return old; \
    }

#define MINI_OS_ATOMIC_CAS_FN(SUFFIX, TYPE, VTYPE) \
    MINI_OS_STATIC_INLINE mini_os_int32_t mini_os_atomic_cas_##SUFFIX(VTYPE *ptr, TYPE *expected, TYPE desired) \
    { \
        mini_os_irq_t irq = mini_os_irq_save(); \
        mini_os_int32_t result; \
        if (*ptr == *expected) { \
            *ptr = desired; \
            result = MINI_OS_TRUE; \
        } else { \
            *expected = *ptr; \
            result = MINI_OS_FALSE; \
        } \
        mini_os_irq_restore(irq); \
        return result; \
    }

#define MINI_OS_ATOMIC_EXCHANGE_FN(SUFFIX, TYPE, VTYPE) \
    MINI_OS_STATIC_INLINE TYPE mini_os_atomic_exchange_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_os_irq_t irq = mini_os_irq_save(); \
        TYPE old = *ptr; \
        *ptr = value; \
        mini_os_irq_restore(irq); \
        return old; \
    }

MINI_OS_ATOMIC_STORE_FN(int8, mini_os_int8_t, mini_os_volatile_int8_t)
MINI_OS_ATOMIC_STORE_FN(uint8, mini_os_uint8_t, mini_os_volatile_uint8_t)
MINI_OS_ATOMIC_STORE_FN(int16, mini_os_int16_t, mini_os_volatile_int16_t)
MINI_OS_ATOMIC_STORE_FN(uint16, mini_os_uint16_t, mini_os_volatile_uint16_t)
MINI_OS_ATOMIC_STORE_FN(int32, mini_os_int32_t, mini_os_volatile_int32_t)
MINI_OS_ATOMIC_STORE_FN(uint32, mini_os_uint32_t, mini_os_volatile_uint32_t)

MINI_OS_ATOMIC_LOAD_FN(int8, mini_os_int8_t, mini_os_volatile_int8_t)
MINI_OS_ATOMIC_LOAD_FN(uint8, mini_os_uint8_t, mini_os_volatile_uint8_t)
MINI_OS_ATOMIC_LOAD_FN(int16, mini_os_int16_t, mini_os_volatile_int16_t)
MINI_OS_ATOMIC_LOAD_FN(uint16, mini_os_uint16_t, mini_os_volatile_uint16_t)
MINI_OS_ATOMIC_LOAD_FN(int32, mini_os_int32_t, mini_os_volatile_int32_t)
MINI_OS_ATOMIC_LOAD_FN(uint32, mini_os_uint32_t, mini_os_volatile_uint32_t)

MINI_OS_ATOMIC_FETCH_ADD_FN(int8, mini_os_int8_t, mini_os_volatile_int8_t)
MINI_OS_ATOMIC_FETCH_ADD_FN(uint8, mini_os_uint8_t, mini_os_volatile_uint8_t)
MINI_OS_ATOMIC_FETCH_ADD_FN(int16, mini_os_int16_t, mini_os_volatile_int16_t)
MINI_OS_ATOMIC_FETCH_ADD_FN(uint16, mini_os_uint16_t, mini_os_volatile_uint16_t)
MINI_OS_ATOMIC_FETCH_ADD_FN(int32, mini_os_int32_t, mini_os_volatile_int32_t)
MINI_OS_ATOMIC_FETCH_ADD_FN(uint32, mini_os_uint32_t, mini_os_volatile_uint32_t)

MINI_OS_ATOMIC_FETCH_SUB_FN(int8, mini_os_int8_t, mini_os_volatile_int8_t)
MINI_OS_ATOMIC_FETCH_SUB_FN(uint8, mini_os_uint8_t, mini_os_volatile_uint8_t)
MINI_OS_ATOMIC_FETCH_SUB_FN(int16, mini_os_int16_t, mini_os_volatile_int16_t)
MINI_OS_ATOMIC_FETCH_SUB_FN(uint16, mini_os_uint16_t, mini_os_volatile_uint16_t)
MINI_OS_ATOMIC_FETCH_SUB_FN(int32, mini_os_int32_t, mini_os_volatile_int32_t)
MINI_OS_ATOMIC_FETCH_SUB_FN(uint32, mini_os_uint32_t, mini_os_volatile_uint32_t)

MINI_OS_ATOMIC_CAS_FN(int8, mini_os_int8_t, mini_os_volatile_int8_t)
MINI_OS_ATOMIC_CAS_FN(uint8, mini_os_uint8_t, mini_os_volatile_uint8_t)
MINI_OS_ATOMIC_CAS_FN(int16, mini_os_int16_t, mini_os_volatile_int16_t)
MINI_OS_ATOMIC_CAS_FN(uint16, mini_os_uint16_t, mini_os_volatile_uint16_t)
MINI_OS_ATOMIC_CAS_FN(int32, mini_os_int32_t, mini_os_volatile_int32_t)
MINI_OS_ATOMIC_CAS_FN(uint32, mini_os_uint32_t, mini_os_volatile_uint32_t)

MINI_OS_ATOMIC_EXCHANGE_FN(int8, mini_os_int8_t, mini_os_volatile_int8_t)
MINI_OS_ATOMIC_EXCHANGE_FN(uint8, mini_os_uint8_t, mini_os_volatile_uint8_t)
MINI_OS_ATOMIC_EXCHANGE_FN(int16, mini_os_int16_t, mini_os_volatile_int16_t)
MINI_OS_ATOMIC_EXCHANGE_FN(uint16, mini_os_uint16_t, mini_os_volatile_uint16_t)
MINI_OS_ATOMIC_EXCHANGE_FN(int32, mini_os_int32_t, mini_os_volatile_int32_t)
MINI_OS_ATOMIC_EXCHANGE_FN(uint32, mini_os_uint32_t, mini_os_volatile_uint32_t)

#undef MINI_OS_ATOMIC_STORE_FN
#undef MINI_OS_ATOMIC_LOAD_FN
#undef MINI_OS_ATOMIC_FETCH_ADD_FN
#undef MINI_OS_ATOMIC_FETCH_SUB_FN
#undef MINI_OS_ATOMIC_CAS_FN
#undef MINI_OS_ATOMIC_EXCHANGE_FN

#define MINI_OS_ATOMIC_STORE(ptr, value, mem) \
    _Generic((ptr), \
        mini_os_volatile_int8_t *:  mini_os_atomic_store_int8, \
        mini_os_volatile_uint8_t *: mini_os_atomic_store_uint8, \
        mini_os_volatile_int16_t *: mini_os_atomic_store_int16, \
        mini_os_volatile_uint16_t *: mini_os_atomic_store_uint16, \
        mini_os_volatile_int32_t *: mini_os_atomic_store_int32, \
        mini_os_volatile_uint32_t *: mini_os_atomic_store_uint32 \
    )((ptr), (value))                    /**<mini-os atomic store*/

#define MINI_OS_ATOMIC_LOAD(ptr, mem) \
    _Generic((ptr), \
        mini_os_volatile_int8_t *:  mini_os_atomic_load_int8, \
        mini_os_volatile_uint8_t *: mini_os_atomic_load_uint8, \
        mini_os_volatile_int16_t *: mini_os_atomic_load_int16, \
        mini_os_volatile_uint16_t *: mini_os_atomic_load_uint16, \
        mini_os_volatile_int32_t *: mini_os_atomic_load_int32, \
        mini_os_volatile_uint32_t *: mini_os_atomic_load_uint32, \
        const mini_os_volatile_int8_t *:  mini_os_atomic_load_int8, \
        const mini_os_volatile_uint8_t *: mini_os_atomic_load_uint8, \
        const mini_os_volatile_int16_t *: mini_os_atomic_load_int16, \
        const mini_os_volatile_uint16_t *: mini_os_atomic_load_uint16, \
        const mini_os_volatile_int32_t *: mini_os_atomic_load_int32, \
        const mini_os_volatile_uint32_t *: mini_os_atomic_load_uint32 \
    )((ptr))                             /**<mini-os atomic load*/

#define MINI_OS_ATOMIC_FETCH_ADD(ptr, value, mem) \
    _Generic((ptr), \
        mini_os_volatile_int8_t *:  mini_os_atomic_fetch_add_int8, \
        mini_os_volatile_uint8_t *: mini_os_atomic_fetch_add_uint8, \
        mini_os_volatile_int16_t *: mini_os_atomic_fetch_add_int16, \
        mini_os_volatile_uint16_t *: mini_os_atomic_fetch_add_uint16, \
        mini_os_volatile_int32_t *: mini_os_atomic_fetch_add_int32, \
        mini_os_volatile_uint32_t *: mini_os_atomic_fetch_add_uint32 \
    )((ptr), (value))                    /**<mini-os atomic fetch add*/

#define MINI_OS_ATOMIC_FETCH_SUB(ptr, value, mem) \
    _Generic((ptr), \
        mini_os_volatile_int8_t *:  mini_os_atomic_fetch_sub_int8, \
        mini_os_volatile_uint8_t *: mini_os_atomic_fetch_sub_uint8, \
        mini_os_volatile_int16_t *: mini_os_atomic_fetch_sub_int16, \
        mini_os_volatile_uint16_t *: mini_os_atomic_fetch_sub_uint16, \
        mini_os_volatile_int32_t *: mini_os_atomic_fetch_sub_int32, \
        mini_os_volatile_uint32_t *: mini_os_atomic_fetch_sub_uint32 \
    )((ptr), (value))                    /**<mini-os atomic fetch sub*/

#define MINI_OS_ATOMIC_CAS(ptr, expected, desired, mem_success, mem_fail) \
    _Generic((ptr), \
        mini_os_volatile_int8_t *:  mini_os_atomic_cas_int8, \
        mini_os_volatile_uint8_t *: mini_os_atomic_cas_uint8, \
        mini_os_volatile_int16_t *: mini_os_atomic_cas_int16, \
        mini_os_volatile_uint16_t *: mini_os_atomic_cas_uint16, \
        mini_os_volatile_int32_t *: mini_os_atomic_cas_int32, \
        mini_os_volatile_uint32_t *: mini_os_atomic_cas_uint32 \
    )((ptr), (expected), (desired))      /**<mini-os atomic compare exchange*/

#define MINI_OS_ATOMIC_EXCHANGE(ptr, value, mem) \
    _Generic((ptr), \
        mini_os_volatile_int8_t *:  mini_os_atomic_exchange_int8, \
        mini_os_volatile_uint8_t *: mini_os_atomic_exchange_uint8, \
        mini_os_volatile_int16_t *: mini_os_atomic_exchange_int16, \
        mini_os_volatile_uint16_t *: mini_os_atomic_exchange_uint16, \
        mini_os_volatile_int32_t *: mini_os_atomic_exchange_int32, \
        mini_os_volatile_uint32_t *: mini_os_atomic_exchange_uint32 \
    )((ptr), (value))                    /**<mini-os atomic exchange*/

#define MINI_OS_ATOMIC_RUNTIME_INIT(p, val) MINI_OS_ATOMIC_STORE((p), (val), MINI_OS_RELAXED)
#define MINI_OS_ASSERT(condition, fmt)              _Static_assert(condition, fmt)                  /**<mini-os assert*/
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*                                      memset (freestanding)                                              */
/*---------------------------------------------------------------------------------------------------------*/
#if defined (__clang__) || defined (__GNUC__)
#define MINI_OS_MEMSET(dst, val, n)           __builtin_memset((dst), (val), (n)) /**<memset: compiler inline expand, word-width stores*/
#else
MINI_OS_STATIC_INLINE void* mini_os_memset_fallback(void* dst, mini_os_int32_t val, mini_os_size_t n)
{
    mini_os_uint8_t* p = (mini_os_uint8_t*)dst;
    mini_os_size_t i;

    for (i = 0u; i < n; i++)
    {
        p[i] = (mini_os_uint8_t)val;
    }
    return dst;
}
#define MINI_OS_MEMSET(dst, val, n)           mini_os_memset_fallback((dst), (val), (n)) /**<memset: pure C fallback*/
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*                                      container_of                                                       */
/*---------------------------------------------------------------------------------------------------------*/
#if defined (__clang__) || defined (__GNUC__)
#define mini_os_container_of(ptr, type, member)                                                     \
    ({                                                                                             \
        const MINI_OS_TYPEOF(((type*)0)->member)* mptr = (ptr);                                   \
        (type*)((char*)mptr - __builtin_offsetof(type, member));                                  \
    })
#else
#define mini_os_container_of(ptr, type, member)                                                     \
    ((type*)((char*)(ptr) - offsetof(type, member)))
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*                              boolify(change any value to bool)                                          */
/*---------------------------------------------------------------------------------------------------------*/
#define mini_os_boolify(val) (!!(val))

#if defined (__cplusplus)
}
#endif

#endif /* REDEF_H */
