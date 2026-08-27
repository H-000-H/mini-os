/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file redef.h
 * @author H-OOO-H
 * @brief mini-os redefinition macros file
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
typedef signed char                         mini_int8_t;                /**<mini-os int8_t*/
typedef signed short                        mini_int16_t;               /**<mini-os int16_t*/
typedef signed int                          mini_int32_t;               /**<mini-os int32_t*/
typedef signed long long                    mini_int64_t;               /**<mini-os int64_t*/
typedef unsigned char                       mini_uint8_t;               /**<mini-os uint8_t*/
typedef unsigned short                      mini_uint16_t;              /**<mini-os uint16_t*/
typedef unsigned int                        mini_uint32_t;              /**<mini-os uint32_t*/
typedef unsigned long long                  mini_uint64_t;              /**<mini-os uint64_t*/
typedef mini_int8_t                         mini_bool_t;                /**<mini-os bool_t*/
typedef size_t                              mini_size_t;                /**<mini-os size_t*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                    volatile Modification base type                                      */
/*---------------------------------------------------------------------------------------------------------*/
typedef volatile signed char                mini_volatile_int8_t;       /**<mini-os volatile int8_t*/
typedef volatile signed short               mini_volatile_int16_t;      /**<mini-os volatile int16_t*/
typedef volatile signed int                 mini_volatile_int32_t;      /**<mini-os volatile int32_t*/
typedef volatile unsigned char              mini_volatile_uint8_t;      /**<mini-os volatile uint8_t*/
typedef volatile unsigned short             mini_volatile_uint16_t;     /**<mini-os volatile uint16_t*/
typedef volatile unsigned int               mini_volatile_uint32_t;     /**<mini-os volatile uint32_t*/

/*---------------------------------------------------------------------------------------------------------*/
/*                                          mini-os-self-type                                              */
/*---------------------------------------------------------------------------------------------------------*/
typedef mini_int32_t                        mini_err_t;                 /**<mini-os err_t*/
typedef signed long                         mini_irq_t;                 /**<mini-os irq_t*/
typedef signed long                         mini_tick_t;               /**<mini-os tick_t*/
typedef signed long                         mini_user_data_t;          /**<mini-os user_data_t*/

#define MINI_TRUE                           (1)                         /**<true*/
#define MINI_FALSE                          (0)                         /**<false*/

#ifdef MINI_NULL_TO_STANDARD
#define MINI_NULL                           ((void *)0)                 /**<null*/
#else
#define MINI_NULL                           (0)                         /**<null*/
#endif

#define MINI_UINT8_MAX                      (0XFF)                       /**<uint8_t max*/
#define MINI_UINT16_MAX                     (0XFFFF)                     /**<uint16_t max*/
#define MINI_UINT32_MAX                     (0xFFFFFFFF)                 /**<uint32_t max*/

/*---------------------------------------------------------------------------------------------------------*/
/*                                  mini-os-gcc-features-macro                                             */
/*---------------------------------------------------------------------------------------------------------*/
#if defined(__clang__) || defined(__GNUC__)
#define MINI_VA_START(x,l)                    __builtin_va_start(x,l)   /**<va_start*/
#define MINI_VA_END(x)                      __builtin_va_end(x)        /**<va_end*/
#define MINI_VA_ARG(x,l)                      __builtin_va_arg(x,l)      /**<va_arg*/
#define MINI_SECTION(x)                      __attribute__((section(x))) /**<section*/
#define MINI_CLZ(x)                         (__builtin_clz(x))          /**<count leading zeros*/
#define MINI_CTZ(x)                         (__builtin_ctz(x))          /**<count trailing zeros*/
#define MINI_POPCOUNT(x)                    (__builtin_popcount(x))     /**<count set bits*/
#define MINI_BSWAP(x)                       (__builtin_bswap(x))        /**<byte swap reversal big endian and little endian*/
#define MINI_ALIGN(x)                       __attribute__((aligned(x))) /**<Compile-time alignment */
#define MINI_UNUSED(x)                      __attribute__((unused))     /**<unused*/
#define MINI_USED(x)                        __attribute__((used))       /**<used*/
#define MINI_NO_RETURN                      __attribute__((noreturn))   /**<no return*/
#define MINI_FORCE_INLINE                   __attribute__((always_inline))/**<Force inline*/
#define MINI_WEAK                           __attribute__((weak))        /**<weak*/
#define MINI_INLINE                         __inline                    /**<inline*/
#define MINI_STATIC                         static                      /**<static*/
#define MINI_STATIC_INLINE                  static __inline             /**<static inline*/
#define MINI_STATIC_FORCE_INLINE            static __attribute__((always_inline))       /**<static force inline*/
#define MINI_TYPEOF(x)                      __typeof__(x)                   /**<typeof*/
#define MINI_CONSTRUCTOR(x) __attribute__((constructor(x)))                 /**<constructor*/
#else
#define MINI_INLINE                         inline                      /**<inline*/
#define MINI_STATIC                         static                      /**<static*/
#define MINI_STATIC_INLINE                  static inline               /**<static inline*/
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*                                  mini-os-interrupt-functions                                            */
/*---------------------------------------------------------------------------------------------------------*/
MINI_STATIC_INLINE void mini_irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

/**
 * @brief Enable interrupts
 */
MINI_STATIC_INLINE void mini_irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

/**
 * @brief Save interrupt state
 */
MINI_STATIC_INLINE mini_irq_t mini_irq_save(void)
{
    mini_irq_t irq_level;
    __asm volatile ("mrs %0, primask\n" "cpsid i\n" : "=r" (irq_level) :: "memory");
    return irq_level;
}

/**
 * @brief Restore interrupt state
 */
MINI_STATIC_INLINE void mini_irq_restore(mini_irq_t irq_level)
{
    __asm volatile ("mrs %0, primask\n" :: "r" (irq_level) : "memory");
}

/*---------------------------------------------------------------------------------------------------------*/
/*                                  mini-os-atomic                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#if defined (__clang__) || defined (__GNUC__)
typedef mini_int8_t  mini_atomic_int8_t;            /**<mini-os atomic int8_t*/
typedef mini_uint8_t mini_atomic_uint8_t;           /**<mini-os atomic uint8_t*/

typedef mini_int16_t mini_atomic_int16_t;           /**<mini-os atomic int16_t*/
typedef mini_uint16_t mini_atomic_uint16_t;          /**<mini-os atomic uint16_t*/

typedef mini_int32_t mini_atomic_int32_t;           /**<mini-os atomic int32_t*/
typedef mini_uint32_t mini_atomic_uint32_t;          /**<mini-os atomic uint32_t*/

#define MINI_RELAXED __ATOMIC_RELAXED               /**<mini-os atomic relaxed*/
#define MINI_ACQUIRE __ATOMIC_ACQUIRE               /**<mini-os atomic acquire*/
#define MINI_RELEASE __ATOMIC_RELEASE               /**<mini-os atomic release*/
#define MINI_ACQ_REL __ATOMIC_ACQ_REL               /**<mini-os atomic acquire-release*/
#define MINI_SEQ_CST __ATOMIC_SEQ_CST               /**<mini-os atomic sequence consistent*/
#define MINI_ATOMIC_STORE(ptr, value, mem) __atomic_store_n((ptr), (value), (mem))               /**<mini-os atomic store*/
#define MINI_ATOMIC_LOAD(ptr, mem) __atomic_load_n((ptr), (mem))                               /**<mini-os atomic load*/
#define MINI_ATOMIC_ADD_FETCH(ptr, value, mem) __atomic_add_fetch((ptr), (value), (mem))       /**<mini-os atomic add fetch*/
#define MINI_ATOMIC_SUB_FETCH(ptr, value, mem) __atomic_sub_fetch((ptr), (value), (mem))       /**<mini-os atomic sub fetch*/
#define MINI_ATOMIC_FETCH_ADD(ptr, value, mem) __atomic_fetch_add((ptr), (value), (mem))       /**<mini-os atomic fetch add*/
#define MINI_ATOMIC_FETCH_SUB(ptr, value, mem) __atomic_fetch_sub((ptr), (value), (mem))       /**<mini-os atomic fetch sub*/
#define MINI_ATOMIC_CAS(ptr, expected, desired, mem_success, mem_fail)\
        __atomic_compare_exchange_n((ptr), (expected), (desired),MINI_TRUE, (mem_success), (mem_fail))       /**<mini-os atomic compare exchange*/
#define MINI_ATOMIC_EXCHANGE(ptr, value, mem) __atomic_exchange_n((ptr), (value), (mem))       /**<mini-os atomic exchange*/
#define MINI_ATOMIC_INIT(ptr,val) MINI_ATOMIC_STORE((ptr), (val), MINI_RELAXED)               /**<mini-os atomic init*/
#else
typedef mini_volatile_int8_t  mini_atomic_int8_t;            /**<mini-os atomic int8_t*/
typedef mini_volatile_uint8_t mini_atomic_uint8_t;           /**<mini-os atomic uint8_t*/

typedef mini_volatile_int16_t mini_atomic_int16_t;           /**<mini-os atomic int16_t*/
typedef mini_volatile_uint16_t mini_atomic_uint16_t;          /**<mini-os atomic uint16_t*/

typedef mini_volatile_int32_t mini_atomic_int32_t;           /**<mini-os atomic int32_t*/
typedef mini_volatile_uint32_t mini_atomic_uint32_t;          /**<mini-os atomic uint32_t*/

#define MINI_RELAXED 0
#define MINI_ACQUIRE 0
#define MINI_RELEASE 0
#define MINI_ACQ_REL 0
#define MINI_SEQ_CST 0

/*---------------------------------------------------------------------------------------------------------*/
/*                          pure ISO C11 atomic fallback (no compiler builtins)                            */
/*---------------------------------------------------------------------------------------------------------*/
/* irq save/restore makes each operation atomic against interrupts. The value-returning operations cannot be
 * written as pure-C expressions, so each is a static inline helper selected with the ISO C11 _Generic
 * keyword. The 'mem' arguments are accepted for API parity but cannot be honored without builtins. */

#define MINI_ATOMIC_STORE_FN(SUFFIX, TYPE, VTYPE) \
    MINI_STATIC_INLINE void mini_atomic_store_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_irq_t irq = mini_irq_save(); \
        *ptr = value; \
        mini_irq_restore(irq); \
    }

#define MINI_ATOMIC_LOAD_FN(SUFFIX, TYPE, VTYPE) \
    MINI_STATIC_INLINE TYPE mini_atomic_load_##SUFFIX(const VTYPE *ptr) \
    { \
        mini_irq_t irq = mini_irq_save(); \
        TYPE value = *ptr; \
        mini_irq_restore(irq); \
        return value; \
    }

#define MINI_ATOMIC_FETCH_ADD_FN(SUFFIX, TYPE, VTYPE) \
    MINI_STATIC_INLINE TYPE mini_atomic_fetch_add_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_irq_t irq = mini_irq_save(); \
        TYPE old = *ptr; \
        *ptr = old + value; \
        mini_irq_restore(irq); \
        return old; \
    }

#define MINI_ATOMIC_FETCH_SUB_FN(SUFFIX, TYPE, VTYPE) \
    MINI_STATIC_INLINE TYPE mini_atomic_fetch_sub_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_irq_t irq = mini_irq_save(); \
        TYPE old = *ptr; \
        *ptr = old - value; \
        mini_irq_restore(irq); \
        return old; \
    }

#define MINI_ATOMIC_CAS_FN(SUFFIX, TYPE, VTYPE) \
    MINI_STATIC_INLINE mini_int32_t mini_atomic_cas_##SUFFIX(VTYPE *ptr, TYPE *expected, TYPE desired) \
    { \
        mini_irq_t irq = mini_irq_save(); \
        mini_int32_t result; \
        if (*ptr == *expected) { \
            *ptr = desired; \
            result = MINI_TRUE; \
        } else { \
            *expected = *ptr; \
            result = MINI_FALSE; \
        } \
        mini_irq_restore(irq); \
        return result; \
    }

#define MINI_ATOMIC_EXCHANGE_FN(SUFFIX, TYPE, VTYPE) \
    MINI_STATIC_INLINE TYPE mini_atomic_exchange_##SUFFIX(VTYPE *ptr, TYPE value) \
    { \
        mini_irq_t irq = mini_irq_save(); \
        TYPE old = *ptr; \
        *ptr = value; \
        mini_irq_restore(irq); \
        return old; \
    }

MINI_ATOMIC_STORE_FN(int8, mini_int8_t, mini_volatile_int8_t)
MINI_ATOMIC_STORE_FN(uint8, mini_uint8_t, mini_volatile_uint8_t)
MINI_ATOMIC_STORE_FN(int16, mini_int16_t, mini_volatile_int16_t)
MINI_ATOMIC_STORE_FN(uint16, mini_uint16_t, mini_volatile_uint16_t)
MINI_ATOMIC_STORE_FN(int32, mini_int32_t, mini_volatile_int32_t)
MINI_ATOMIC_STORE_FN(uint32, mini_uint32_t, mini_volatile_uint32_t)

MINI_ATOMIC_LOAD_FN(int8, mini_int8_t, mini_volatile_int8_t)
MINI_ATOMIC_LOAD_FN(uint8, mini_uint8_t, mini_volatile_uint8_t)
MINI_ATOMIC_LOAD_FN(int16, mini_int16_t, mini_volatile_int16_t)
MINI_ATOMIC_LOAD_FN(uint16, mini_uint16_t, mini_volatile_uint16_t)
MINI_ATOMIC_LOAD_FN(int32, mini_int32_t, mini_volatile_int32_t)
MINI_ATOMIC_LOAD_FN(uint32, mini_uint32_t, mini_volatile_uint32_t)

MINI_ATOMIC_FETCH_ADD_FN(int8, mini_int8_t, mini_volatile_int8_t)
MINI_ATOMIC_FETCH_ADD_FN(uint8, mini_uint8_t, mini_volatile_uint8_t)
MINI_ATOMIC_FETCH_ADD_FN(int16, mini_int16_t, mini_volatile_int16_t)
MINI_ATOMIC_FETCH_ADD_FN(uint16, mini_uint16_t, mini_volatile_uint16_t)
MINI_ATOMIC_FETCH_ADD_FN(int32, mini_int32_t, mini_volatile_int32_t)
MINI_ATOMIC_FETCH_ADD_FN(uint32, mini_uint32_t, mini_volatile_uint32_t)

MINI_ATOMIC_FETCH_SUB_FN(int8, mini_int8_t, mini_volatile_int8_t)
MINI_ATOMIC_FETCH_SUB_FN(uint8, mini_uint8_t, mini_volatile_uint8_t)
MINI_ATOMIC_FETCH_SUB_FN(int16, mini_int16_t, mini_volatile_int16_t)
MINI_ATOMIC_FETCH_SUB_FN(uint16, mini_uint16_t, mini_volatile_uint16_t)
MINI_ATOMIC_FETCH_SUB_FN(int32, mini_int32_t, mini_volatile_int32_t)
MINI_ATOMIC_FETCH_SUB_FN(uint32, mini_uint32_t, mini_volatile_uint32_t)

MINI_ATOMIC_CAS_FN(int8, mini_int8_t, mini_volatile_int8_t)
MINI_ATOMIC_CAS_FN(uint8, mini_uint8_t, mini_volatile_uint8_t)
MINI_ATOMIC_CAS_FN(int16, mini_int16_t, mini_volatile_int16_t)
MINI_ATOMIC_CAS_FN(uint16, mini_uint16_t, mini_volatile_uint16_t)
MINI_ATOMIC_CAS_FN(int32, mini_int32_t, mini_volatile_int32_t)
MINI_ATOMIC_CAS_FN(uint32, mini_uint32_t, mini_volatile_uint32_t)

MINI_ATOMIC_EXCHANGE_FN(int8, mini_int8_t, mini_volatile_int8_t)
MINI_ATOMIC_EXCHANGE_FN(uint8, mini_uint8_t, mini_volatile_uint8_t)
MINI_ATOMIC_EXCHANGE_FN(int16, mini_int16_t, mini_volatile_int16_t)
MINI_ATOMIC_EXCHANGE_FN(uint16, mini_uint16_t, mini_volatile_uint16_t)
MINI_ATOMIC_EXCHANGE_FN(int32, mini_int32_t, mini_volatile_int32_t)
MINI_ATOMIC_EXCHANGE_FN(uint32, mini_uint32_t, mini_volatile_uint32_t)

#undef MINI_ATOMIC_STORE_FN
#undef MINI_ATOMIC_LOAD_FN
#undef MINI_ATOMIC_FETCH_ADD_FN
#undef MINI_ATOMIC_FETCH_SUB_FN
#undef MINI_ATOMIC_CAS_FN
#undef MINI_ATOMIC_EXCHANGE_FN

#define MINI_ATOMIC_STORE(ptr, value, mem) \
    _Generic((ptr), \
        mini_volatile_int8_t *:  mini_atomic_store_int8, \
        mini_volatile_uint8_t *: mini_atomic_store_uint8, \
        mini_volatile_int16_t *: mini_atomic_store_int16, \
        mini_volatile_uint16_t *: mini_atomic_store_uint16, \
        mini_volatile_int32_t *: mini_atomic_store_int32, \
        mini_volatile_uint32_t *: mini_atomic_store_uint32 \
    )((ptr), (value))                    /**<mini-os atomic store*/

#define MINI_ATOMIC_LOAD(ptr, mem) \
    _Generic((ptr), \
        mini_volatile_int8_t *:  mini_atomic_load_int8, \
        mini_volatile_uint8_t *: mini_atomic_load_uint8, \
        mini_volatile_int16_t *: mini_atomic_load_int16, \
        mini_volatile_uint16_t *: mini_atomic_load_uint16, \
        mini_volatile_int32_t *: mini_atomic_load_int32, \
        mini_volatile_uint32_t *: mini_atomic_load_uint32, \
        const mini_volatile_int8_t *:  mini_atomic_load_int8, \
        const mini_volatile_uint8_t *: mini_atomic_load_uint8, \
        const mini_volatile_int16_t *: mini_atomic_load_int16, \
        const mini_volatile_uint16_t *: mini_atomic_load_uint16, \
        const mini_volatile_int32_t *: mini_atomic_load_int32, \
        const mini_volatile_uint32_t *: mini_atomic_load_uint32 \
    )((ptr))                             /**<mini-os atomic load*/

#define MINI_ATOMIC_FETCH_ADD(ptr, value, mem) \
    _Generic((ptr), \
        mini_volatile_int8_t *:  mini_atomic_fetch_add_int8, \
        mini_volatile_uint8_t *: mini_atomic_fetch_add_uint8, \
        mini_volatile_int16_t *: mini_atomic_fetch_add_int16, \
        mini_volatile_uint16_t *: mini_atomic_fetch_add_uint16, \
        mini_volatile_int32_t *: mini_atomic_fetch_add_int32, \
        mini_volatile_uint32_t *: mini_atomic_fetch_add_uint32 \
    )((ptr), (value))                    /**<mini-os atomic fetch add*/

#define MINI_ATOMIC_FETCH_SUB(ptr, value, mem) \
    _Generic((ptr), \
        mini_volatile_int8_t *:  mini_atomic_fetch_sub_int8, \
        mini_volatile_uint8_t *: mini_atomic_fetch_sub_uint8, \
        mini_volatile_int16_t *: mini_atomic_fetch_sub_int16, \
        mini_volatile_uint16_t *: mini_atomic_fetch_sub_uint16, \
        mini_volatile_int32_t *: mini_atomic_fetch_sub_int32, \
        mini_volatile_uint32_t *: mini_atomic_fetch_sub_uint32 \
    )((ptr), (value))                    /**<mini-os atomic fetch sub*/

#define MINI_ATOMIC_CAS(ptr, expected, desired, mem_success, mem_fail) \
    _Generic((ptr), \
        mini_volatile_int8_t *:  mini_atomic_cas_int8, \
        mini_volatile_uint8_t *: mini_atomic_cas_uint8, \
        mini_volatile_int16_t *: mini_atomic_cas_int16, \
        mini_volatile_uint16_t *: mini_atomic_cas_uint16, \
        mini_volatile_int32_t *: mini_atomic_cas_int32, \
        mini_volatile_uint32_t *: mini_atomic_cas_uint32 \
    )((ptr), (expected), (desired))      /**<mini-os atomic compare exchange*/

#define MINI_ATOMIC_EXCHANGE(ptr, value, mem) \
    _Generic((ptr), \
        mini_volatile_int8_t *:  mini_atomic_exchange_int8, \
        mini_volatile_uint8_t *: mini_atomic_exchange_uint8, \
        mini_volatile_int16_t *: mini_atomic_exchange_int16, \
        mini_volatile_uint16_t *: mini_atomic_exchange_uint16, \
        mini_volatile_int32_t *: mini_atomic_exchange_int32, \
        mini_volatile_uint32_t *: mini_atomic_exchange_uint32 \
    )((ptr), (value))                    /**<mini-os atomic exchange*/

#define MINI_ATOMIC_RUNTIME_INIT(p, val) MINI_ATOMIC_STORE((p), (val), MINI_RELAXED)
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*                                      container_of                                                       */
/*---------------------------------------------------------------------------------------------------------*/
#if defined (__clang__) || defined (__GNUC__)
#define mini_container_of(ptr, type, member)                                                            \
    ({                                                                                             \
        const MINI_TYPEOF(((type*)0)->member)* mptr = (ptr);                                          \
        (type*)((char*)mptr - __builtin_offsetof(type, member));                                 \
    })
#else
#define mini_container_of(ptr, type, member)                                                            \
    ((type*)((char*)(ptr) - offsetof(type, member)))
#endif

/*---------------------------------------------------------------------------------------------------------*/
/*                              boolify(change any value to bool)                                          */
/*---------------------------------------------------------------------------------------------------------*/
#define mini_boolify(val) (!!(val))

#if defined (__cplusplus)
}
#endif

#endif /* REDEF_H */
