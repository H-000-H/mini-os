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
#define MINI_OS_THREADS_NAME_LEN CONFIG_THREADS_NAME_LEN   /**< max thread name length (incl. NUL) */
#elif defined(MINI_OS_THREADS_NAME_LEN)
/* MINI_OS_THREADS_NAME_LEN pre-defined externally , keep it */
#else
#define MINI_OS_THREADS_NAME_LEN 32                          /**< max thread name length (incl. NUL) */
#endif

#ifdef CONFIG_QUEUE_NAME_LEN
#define MINI_OS_QUEUE_NAME_LEN CONFIG_QUEUE_NAME_LEN    /**< max queue name length (incl. NUL) */
#elif defined(MINI_OS_QUEUE_NAME_LEN)
/* MINI_OS_QUEUE_NAME_LEN pre-defined externally , keep it */
#else
#define MINI_OS_QUEUE_NAME_LEN 32                        /**< max queue name length (incl. NUL) */
#endif

#ifdef CONFIG_SEMAPHORE_NAME_LEN
#define MINI_OS_SEMAPHORE_NAME_LEN CONFIG_SEMAPHORE_NAME_LEN    /**< max semaphore name length (incl. NUL) */
#elif defined(MINI_OS_SEMAPHORE_NAME_LEN)
/* MINI_OS_SEMAPHORE_NAME_LEN pre-defined externally , keep it */
#else
#define MINI_OS_SEMAPHORE_NAME_LEN 32                        /**< max semaphore name length (incl. NUL) */
#endif

#define MINI_OS_IDLE_THREAD_NAME "idle_thread"  /**< idle thread name */

#define MINI_OS_IDLE_THREAD_PRIORITY 1          /**< idle thread priority */

#ifdef CONFIG_MINI_IDLE_THREAD_STACK_SIZE
#define MINI_OS_IDLE_THREAD_STACK_SIZE CONFIG_MINI_IDLE_THREAD_STACK_SIZE   /**< idle thread stack size in bytes */
#elif defined(MINI_OS_IDLE_THREAD_STACK_SIZE)
/* MINI_OS_IDLE_THREAD_STACK_SIZE pre-defined externally , keep it */
#else
#define MINI_OS_IDLE_THREAD_STACK_SIZE 512                                /**< idle thread stack size in bytes */
#endif

#define MINI_OS_IDLE_THREAD_CONSTRUCTOR 105     /**< idle thread constructor priority */
#define MINI_OS_FIND_BY_NAME_CONSTRUCTOR 110    /**< find by name constructor priority */
#define MINI_OS_SEMAPHORE_REGISTRY_CONSTRUCTOR 111  /**< semaphore by-name registry constructor priority */
#define MINI_OS_FPU_ENABLE_CONSTRUCTOR 100      /**< FPU enable constructor priority (0-100 reserved; runs first) */
#define MINI_OS_CPU_PROBE_CONSTRUCTOR 101       /**< CPUID probe constructor priority (fails fast before any thread runs) */
#define MINI_OS_STACK_SENTINEL_CONSTRUCTOR 102  /**< stack sentinel constructor priority (after CPU probe, before threads) */

/**
 * @brief Enable stack overflow detection on the system (MSP) stack
 * @note Off by default; set CONFIG_MINI_OS_STACK_OVERFLOW_CHECK=1 to enable.
 *       Plants a magic word at the stack's low boundary (linker symbol
 *       __mini_os_heap_end from mini-os-heap.ld, where heap ends and the
 *       system stack begins); the idle thread re-checks it every loop and
 *       halts when the stack ran over the boundary into the heap.
 */
#ifdef CONFIG_MINI_OS_STACK_OVERFLOW_CHECK
#define MINI_OS_STACK_OVERFLOW_CHECK CONFIG_MINI_OS_STACK_OVERFLOW_CHECK
#elif defined(MINI_OS_STACK_OVERFLOW_CHECK)
/* MINI_OS_STACK_OVERFLOW_CHECK pre-defined externally , keep it */
#else
#define MINI_OS_STACK_OVERFLOW_CHECK 0
#endif

#define MINI_OS_STACK_MAGIC 0x060815U           /**< stack/ overflow sentinel word */

#ifdef CONFIG_MINI_EXECUTION_SLAB_CHECK_SIZE
#define MINI_OS_SLAB_CHECK_SIZE CONFIG_MINI_EXECUTION_SLAB_CHECK_SIZE   /**< slab check size for execution (0 = disabled) */
#elif defined(MINI_OS_SLAB_CHECK_SIZE)
/* MINI_OS_SLAB_CHECK_SIZE pre-defined externally , keep it */
#else
#define MINI_OS_SLAB_CHECK_SIZE 0                                   /**< slab check size for execution (0 = disabled) */
#endif

#ifdef CONFIG_MINI_OS_PRIORITY
#define MINI_OS_PRIORITY CONFIG_MINI_OS_PRIORITY   /**< number of priorities (smaller number = higher priority) */
#elif defined(MINI_OS_PRIORITY)
/* MINI_OS_PRIORITY pre-defined externally , keep it */
#else
#define MINI_OS_PRIORITY 32                          /**< number of priorities (smaller number = higher priority) */
#endif

#ifdef CONFIG_MINI_OS_THREAD_MIN_STACK_SIZE
#define MINI_OS_THREAD_MIN_STACK_SIZE CONFIG_MINI_OS_THREAD_MIN_STACK_SIZE   /**< minimum stack size enforced at thread create */
#elif defined(MINI_OS_THREAD_MIN_STACK_SIZE)
/* MINI_OS_THREAD_MIN_STACK_SIZE pre-defined externally , keep it */
#else
#define MINI_OS_THREAD_MIN_STACK_SIZE 256                                   /**< minimum stack size enforced at thread create */
#endif
/**
 * @brief Enable thread detach/join support (adds per-thread join fields)
 * @note Off by default; set CONFIG_MINI_OS_THREAD_DETACH=1 in config.h to enable
 */
#ifdef CONFIG_MINI_OS_THREAD_DETACH
#define MINI_OS_THREAD_DETACH CONFIG_MINI_OS_THREAD_DETACH
#elif defined(MINI_OS_THREAD_DETACH)
/* MINI_OS_THREAD_DETACH pre-defined externally , keep it */
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
#elif defined(MINI_OS_ARCH)
/* MINI_OS_ARCH pre-defined externally , keep it */
#else
#define MINI_OS_ARCH MINI_OS_ARCH_M4    /**< default architecture id (Cortex-M4) */
#endif

/* Per-architecture constants derived from MINI_OS_ARCH (used by port.S) */
#if MINI_OS_ARCH == MINI_OS_ARCH_M0
#define MINI_OS_NVIC_PRIO_BITS 2        /**< Cortex-M0/M0+: 2 NVIC priority bits */
#define MINI_OS_ARCH_HAS_FPU 0          /**< Cortex-M0/M0+: no FPU */
#elif MINI_OS_ARCH == MINI_OS_ARCH_M3
#define MINI_OS_NVIC_PRIO_BITS 4        /**< Cortex-M3: 4 NVIC priority bits */
#define MINI_OS_ARCH_HAS_FPU 0          /**< Cortex-M3: no FPU */
#elif MINI_OS_ARCH == MINI_OS_ARCH_M4
#define MINI_OS_NVIC_PRIO_BITS 4        /**< Cortex-M4: 4 NVIC priority bits */
#define MINI_OS_ARCH_HAS_FPU 1          /**< Cortex-M4: optional single-precision FPU */
#elif MINI_OS_ARCH == MINI_OS_ARCH_M7
#define MINI_OS_NVIC_PRIO_BITS 4        /**< Cortex-M7: 4 NVIC priority bits */
#define MINI_OS_ARCH_HAS_FPU 1          /**< Cortex-M7: optional single/double-precision FPU */
/* L1 cache maintenance flags for mini_os_dcache_ops (bitmask) */
#define MINI_OS_CACHE_FLUSH      (1 << 0)   /**< clean: write dirty lines back to memory */
#define MINI_OS_CACHE_INVALIDATE (1 << 1)   /**< invalidate: drop the cached copies */
#define MINI_OS_CACHE_LINESIZE   32U        /**< Cortex-M7 L1 cache line size in bytes */
#else
#error "unsupported MINI_OS_ARCH value"
#endif

/* Critical-section interrupt masking policy. By default the critical section
 * API masks ALL interrupts via PRIMASK. Defining MINI_OS_IRQ_MAX_SYSCALL_PRIORITY
 * switches M3/M4/M7 critical sections to BASEPRI: only interrupts whose NVIC
 * priority number is >= this value are masked, more urgent ones (numerically
 * lower) can still preempt the kernel (FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY
 * style). Cortex-M0 has no BASEPRI hardware and always uses PRIMASK. */
#ifdef CONFIG_MINI_OS_IRQ_MAX_SYSCALL_PRIORITY
#define MINI_OS_IRQ_MAX_SYSCALL_PRIORITY CONFIG_MINI_OS_IRQ_MAX_SYSCALL_PRIORITY   /**< BASEPRI threshold (NVIC priority number) */
#elif defined(MINI_OS_IRQ_MAX_SYSCALL_PRIORITY)
/* MINI_OS_IRQ_MAX_SYSCALL_PRIORITY pre-defined externally , keep it */
#else
/* not defined by default: critical sections mask all interrupts via PRIMASK */
#endif

#if defined(MINI_OS_IRQ_MAX_SYSCALL_PRIORITY)
#if MINI_OS_ARCH == MINI_OS_ARCH_M0
#error "MINI_OS_IRQ_MAX_SYSCALL_PRIORITY: Cortex-M0 has no BASEPRI hardware, remove it"
#endif
#if MINI_OS_IRQ_MAX_SYSCALL_PRIORITY == 0
#error "MINI_OS_IRQ_MAX_SYSCALL_PRIORITY must be non-zero (0 would mask nothing)"
#endif
#if MINI_OS_IRQ_MAX_SYSCALL_PRIORITY >= (1 << MINI_OS_NVIC_PRIO_BITS)
#error "MINI_OS_IRQ_MAX_SYSCALL_PRIORITY exceeds the NVIC priority range"
#endif
#define MINI_OS_IRQ_USE_BASEPRI 1   /**< critical sections mask via BASEPRI threshold */

/* BASEPRI stores the threshold in the upper MINI_OS_NVIC_PRIO_BITS bits of its
 * byte-wide register field, shift the priority number into register position.
 * Used by both redef.h callers and the port.S implementation. */
#define MINI_OS_IRQ_BASEPRI_THRESHOLD \
    ((MINI_OS_IRQ_MAX_SYSCALL_PRIORITY) << (8U - MINI_OS_NVIC_PRIO_BITS))   /**< BASEPRI register value for the threshold */
#else
#define MINI_OS_IRQ_USE_BASEPRI 0   /**< critical sections mask everything via PRIMASK */
#endif

/**
 * @brief Enable FPU context save/restore in the PendSV switch (M4/M7 only)
 * @note Off by default; set CONFIG_MINI_OS_USE_FPU=1 in config.h to enable.
 *       Adds one flag word to every thread's initial stack frame and saves
 *       s16-s31 on switch when the thread used the FPU.
 */

#define MINI_OS_NO_FPU 0
#define MINI_OS_FPU 1
#ifdef CONFIG_MINI_OS_USE_FPU
#define MINI_OS_USE_FPU CONFIG_MINI_OS_USE_FPU
#elif defined(MINI_OS_USE_FPU)
/* MINI_OS_USE_FPU pre-defined externally , keep it */
#else
#define MINI_OS_USE_FPU MINI_OS_NO_FPU
#endif

/* Default idle stack size for threads */
#ifdef CONFIG_MINI_OS_DEFAULT_IDLE_STACK_SIZE
#define MINI_OS_DEFAULT_IDLE_STACK_SIZE CONFIG_MINI_OS_DEFAULT_IDLE_STACK_SIZE   /**< default idle stack size in bytes */
#elif defined(MINI_OS_DEFAULT_IDLE_STACK_SIZE)
/* MINI_OS_DEFAULT_IDLE_STACK_SIZE pre-defined externally , keep it */
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
#elif defined(MINI_OS_TICK_WHEEL)
/* MINI_OS_TICK_WHEEL pre-defined externally , keep it */
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
#elif defined(MINI_OS_DEFAULT_SYSTICK)
/* MINI_OS_DEFAULT_SYSTICK pre-defined externally , keep it */
#else
#define MINI_OS_DEFAULT_SYSTICK 1000U                                /**< OS tick rate in Hz */
#endif

/**
 * @brief CPU clock frequency in Hz, used to derive the SysTick reload value
 * @note reload = (MINI_OS_CPU_CLOCK_HZ / 1000) * ticks_per_ms - 1
 */
#ifdef CONFIG_MINI_OS_CPU_CLOCK_HZ
#define MINI_OS_CPU_CLOCK_HZ CONFIG_MINI_OS_CPU_CLOCK_HZ    /**< CPU clock in Hz */
#elif defined(MINI_OS_CPU_CLOCK_HZ)
/* MINI_OS_CPU_CLOCK_HZ pre-defined externally , keep it */
#else
#define MINI_OS_CPU_CLOCK_HZ 72000000U                         /**< CPU clock in Hz */
#endif
#endif /* MINI_CONFIG_H */

/**
 * @brief open time slice configuration
 */
#ifdef CONFIG_MINI_OS_TIME_SLICE
#define MINI_OS_TIME_SLICE CONFIG_MINI_OS_TIME_SLICE
#elif defined(MINI_OS_TIME_SLICE)
/* MINI_OS_TIME_SLICE pre-defined externally , keep it */
#else
#define MINI_OS_TIME_SLICE 0
#endif

/**
 * @brief enable thread find by name
 */
#ifdef CONFIG_MINI_OS_FIND_BY_NAME
#define MINI_OS_FIND_BY_NAME CONFIG_MINI_OS_FIND_BY_NAME
#elif defined(MINI_OS_FIND_BY_NAME)
/* MINI_OS_FIND_BY_NAME pre-defined externally , keep it */
#else
#define MINI_OS_FIND_BY_NAME 0
#endif

/**
 * @brief enable event group support
 */
#ifdef CONFIG_MINI_OS_EVENT
#define MINI_OS_EVENT CONFIG_MINI_OS_EVENT
#elif defined(MINI_OS_EVENT)
/* MINI_OS_EVENT pre-defined externally , keep it */
#else
#define MINI_OS_EVENT 0
#endif
