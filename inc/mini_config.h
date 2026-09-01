/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mini_config.h
 * @brief mini-os configuration heaper - only include definitions
 * @author H-000-H
 */
#ifndef MINI_CONFIG_H
#define MINI_CONFIG_H

/**
 * @details
 * - Include <config.h> and <compiler_compat.h> if they exist that identify using mini-tree's kconfig system
 * - couldn't include <compiler_compat.h> .if included, compiler_compat.h it has conflicts
 */
#if __has_include(<config.h>) && __has_include(<compiler_compat.h>)
#include <config.h>
#endif

/**
 * @brief Define MINI_OS_NULL_TO_STANDARD to use standard NULL macro instead of 0
 */
#define MINI_OS_NULL_TO_STANDARD

/**
 * @brief Define CONFIG_THREADS_NAME_MAX to set the maximum length of thread names
 */
#ifdef CONFIG_THREADS_NAME_LEN
#define THREADS_NAME_LEN CONFIG_THREADS_NAME_LEN   /**< max thread name length (incl. NUL) */
#else
#define THREADS_NAME_LEN 32                          /**< max thread name length (incl. NUL) */
#endif

#ifdef CONFIG_QUEUE_NAME_LEN
#define QUEUE_NAME_LEN CONFIG_QUEUE_NAME_LEN    /**< max queue name length (incl. NUL) */
#else
#define QUEUE_NAME_LEN 32                        /**< max queue name length (incl. NUL) */
#endif

#define MINI_OS_IDLE_THREAD_NAME "idle_thread"  /**< idle thread name */

#define MINI_OS_IDLE_THREAD_PRIORITY 1          /**< idle thread priority */

#ifdef CONFIG_MINI_IDLE_THREAD_STACK_SIZE
#define MINI_OS_IDLE_THREAD_STACK_SIZE CONFIG_MINI_IDLE_THREAD_STACK_SIZE   /**< idle thread stack size in bytes */
#else
#define MINI_OS_IDLE_THREAD_STACK_SIZE 512                                /**< idle thread stack size in bytes */
#endif

#define MINI_OS_IDLE_THREAD_CONSTRUCTOR 105     /**< idle thread constructor priority */
#define MINI_OS_FIND_BY_NAME_CONSTRUCTOR 110    /**< find by name constructor priority */

#ifdef CONFIG_MINI_EXECUTION_SLAB_CHECK_SIZE
#define MINI_EXECUTION_SLAB_CHECK_SIZE CONFIG_MINI_EXECUTION_SLAB_CHECK_SIZE   /**< slab check size for execution (0 = disabled) */
#else
#define MINI_EXECUTION_SLAB_CHECK_SIZE 0                                   /**< slab check size for execution (0 = disabled) */
#endif

#ifdef CONFIG_MINI_OS_PRIORITY
#define MINI_OS_PRIORITY CONFIG_MINI_OS_PRIORITY   /**< number of priorities (smaller number = higher priority) */
#else
#define MINI_OS_PRIORITY 32                          /**< number of priorities (smaller number = higher priority) */
#endif

#ifdef CONFIG_MINI_OS_THREAD_MIN_STACK_SIZE
#define MINI_OS_THREAD_MIN_STACK_SIZE CONFIG_MINI_OS_THREAD_MIN_STACK_SIZE   /**< minimum stack size enforced at thread create */
#else
#define MINI_OS_THREAD_MIN_STACK_SIZE 256                                   /**< minimum stack size enforced at thread create */
#endif
/**
 * @brief Enable thread detach/join support (adds per-thread join fields)
 * @note Off by default; set CONFIG_MINI_OS_THREAD_DETACH=1 in config.h to enable
 */
#ifdef CONFIG_MINI_OS_THREAD_DETACH
#define MINI_OS_THREAD_DETACH CONFIG_MINI_OS_THREAD_DETACH
#else
#define MINI_OS_THREAD_DETACH 0
#endif

/* Number of 32-bit ready-group words (32 priorities per word) */
#ifdef CONFIG_MINI_OS_PRIORITY
#define MINI_OS_PRIORITY_NUM (CONFIG_MINI_OS_PRIORITY / 32)   /**< number of 32-bit ready-group words */
#else
#define MINI_OS_PRIORITY_NUM (MINI_OS_PRIORITY / 32)            /**< number of 32-bit ready-group words */
#endif

/* Sub-groups per priority (time-slice granularity within one priority) */
#ifdef CONFIG_MINI_OS_SAME_PRIORITY
#define MINI_OS_SAME_PRIORITY CONFIG_MINI_OS_SAME_PRIORITY   /**< sub-groups per priority (time-slice granularity) */
#else
#define MINI_OS_SAME_PRIORITY (MINI_OS_PRIORITY / 8)            /**< sub-groups per priority (time-slice granularity) */
#endif

/* Architecture selector. Numeric so it can be used in preprocessor #if
 * comparisons (e.g. port.S selects per-core constants). */
#define MINI_OS_ARCH_M0 0    /**< Cortex-M0 architecture id */
#define MINI_OS_ARCH_M3 1    /**< Cortex-M3 architecture id */
#define MINI_OS_ARCH_M4 2    /**< Cortex-M4 architecture id */
#define MINI_OS_ARCH_M7 3    /**< Cortex-M7 architecture id */

#ifdef CONFIG_ARCH
#define MINI_OS_ARCH CONFIG_ARCH    /**< selected architecture id */
#else
#define MINI_OS_ARCH MINI_OS_ARCH_M3    /**< selected architecture id */
#endif

/* Default idle stack size for threads */
#ifdef CONFIG_MINI_OS_DEFAULT_IDLE_STACK_SIZE
#define MINI_OS_DEFAULT_IDLE_STACK_SIZE CONFIG_MINI_OS_DEFAULT_IDLE_STACK_SIZE   /**< default idle stack size in bytes */
#else
#define MINI_OS_DEFAULT_IDLE_STACK_SIZE 128                                     /**< default idle stack size in bytes */
#endif


#define MINI_OS_CONTROL_REGISTER_MSP_PRIVILEGE 0U   /**< CONTROL: MSP, privileged thread mode */
#define MINI_OS_CONTROL_REGISTER_MSP_USER 1U        /**< CONTROL: MSP, unprivileged thread mode */
#define MINI_OS_CONTROL_REGISTER_PSP_PRIVILEGE 2U   /**< CONTROL: PSP, privileged thread mode */
#define MINI_OS_CONTROL_REGISTER_PSP_USER 3U        /**< CONTROL: PSP, unprivileged thread mode */
#define MINI_OS_NONE_THREAD_TO_RESTORE 0U           /**< marker: no thread to restore on first switch */

#ifdef CONFIG_MINI_OS_TICK_WHEEL
#define MINI_OS_TICK_WHEEL CONFIG_MINI_OS_TICK_WHEEL    /**< tick wheel size (power of 2, number of slots) */
#else
#define MINI_OS_TICK_WHEEL 32U                              /**< tick wheel size (power of 2, number of slots) */
#endif

#define MINI_OS_TICK_WHEEL_MASK (MINI_OS_TICK_WHEEL - 1)    /**< tick wheel slot mask */

#ifdef CONFIG_MINI_OS_LONG_TIME
#define MINI_OS_LONG_TIME    /**< enable 64-bit long tick counting (overflow counter) */
#endif

/**
 * @brief OS tick rate in Hz (default 1000 = one tick per millisecond)
 * @note value is the tick frequency, not a period; used by the tick/ms conversion macros
 */
#ifdef CONFIG_MINI_OS_DEFAULT_SYSTICK
#define MINI_OS_DEFAULT_SYSTICK CONFIG_MINI_OS_DEFAULT_SYSTICK    /**< OS tick rate in Hz */
#else
#define MINI_OS_DEFAULT_SYSTICK 1000U                                /**< OS tick rate in Hz */
#endif

/**
 * @brief CPU clock frequency in Hz, used to derive the SysTick reload value
 * @note reload = (MINI_OS_CPU_CLOCK_HZ / 1000) * ticks_per_ms - 1
 */
#ifdef CONFIG_MINI_OS_CPU_CLOCK_HZ
#define MINI_OS_CPU_CLOCK_HZ CONFIG_MINI_OS_CPU_CLOCK_HZ    /**< CPU clock in Hz */
#else
#define MINI_OS_CPU_CLOCK_HZ 72000000U                         /**< CPU clock in Hz */
#endif
#endif /* MINI_CONFIG_H */

/**
 * @brief open time slice configuration
 */
#ifdef CONFIG_MINI_OS_TIME_SLICE
#define MINI_OS_TIME_SLICE CONFIG_MINI_OS_TIME_SLICE
#else
#define MINI_OS_TIME_SLICE 0
#endif

/**
 * @brief enable thread find by name
 */
#ifdef CONFIG_MINI_OS_FIND_BY_NAME
#define MINI_OS_FIND_BY_NAME CONFIG_MINI_OS_FIND_BY_NAME
#else
#define MINI_OS_FIND_BY_NAME 0
#endif
