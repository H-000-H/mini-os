/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file schedule.h
 * @brief Scheduling functions
 * @author H-000-H
 */
#ifndef SCHEDULE_H
#define SCHEDULE_H
#include "err.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "list.h"
#include "redef.h"
#include "thread.h"
#include "mini_config.h"
/**
 * @brief Convert ticks to milliseconds
 * @param[in] ticks tick count
 * @return elapsed time in ms
 * @note works for any MINI_OS_DEFAULT_SYSTICK (integer math)
 */
#define MINI_OS_TICK_TO_MS(ticks) \
    (((mini_os_uint32_t)(ticks) * 1000u) / MINI_OS_DEFAULT_SYSTICK)

/**
 * @brief Convert milliseconds to ticks
 * @param[in] ms time in milliseconds
 * @return tick count
 * @note works for any MINI_OS_DEFAULT_SYSTICK (integer math)
 */
#define MINI_OS_MS_TO_TICK(ms) \
    (((mini_os_uint32_t)(ms) * MINI_OS_DEFAULT_SYSTICK) / 1000u)

/**
 * @brief Get the current tick count
 * @return current tick count
 * @note user must to write this function
 */
MINI_OS_STATIC_INLINE mini_os_tick_t mini_os_get_ticks(void)
{
    return MINI_OS_ERR_NOTSUPP;
}
extern mini_os_uint32_t g_priority; /**< ready/running bitmap: bit i set = priority i has a ready or running thread (smaller number = higher priority) */

extern mini_os_list_t g_ready_running_list[MINI_OS_PRIORITY]; /**< ready/running list head per priority (running threads stay linked) */
typedef struct mini_os_schedule mini_os_schedule_t;
/**
 * @brief Scheduling structure
 * @note only used by kernel
 */
struct mini_os_schedule
{
    mini_os_tick_t  init_tick;              /**< init tick count */
    mini_os_tick_t  remain_tick;            /**< remaining tick count */
    mini_os_uint8_t init_priority;          /**< initial priority */
    mini_os_uint8_t current_priority;       /**< current priority */
};
/**
 * @brief Initialize the scheduler
 * @return 0 on success, negative error code on failure
 */
mini_os_err_t mini_os_schedule_init(void);
/**
 * @brief Start the scheduler
 * @return 0 on success, negative error code on failure
 */
mini_os_err_t mini_os_schedule_start(void);
/**
 * @brief Yield the current thread
 * @return 0 on success, negative error code on failure
 * @note only used by the kernel user cannot call this function directly
 */
mini_os_err_t mini_os_schedule_yield(void);
/**
 * @brief Delay the current thread for 'ticks' ticks
 * @param[in] ticks number of ticks to delay (0 returns immediately)
 * @note kernel API, used by mini_os_thread_delay_tick()
 */
void mini_os_schedule_delay(mini_os_uint32_t ticks);
/**
 * @brief Add a thread to the ready/running queue
 * @param[in] thread The thread to add
 * @return 0 on success, negative error code on failure
 */
mini_os_err_t mini_os_add_thread_to_ready_running_list(mini_os_thread_t *thread);
/**
 * @brief Remove a thread from the ready/running queue
 * @param[in] thread The thread to remove
 * @return 0 on success, negative error code on failure
 */
mini_os_err_t mini_os_remove_thread_from_ready_running_list(mini_os_thread_t *thread);


/**
 * @brief Remove a thread from the time-wheel blocked list
 * @param[in] thread thread to remove (must be MINI_OS_THREAD_STATE_BLOCKED)
 * @return MINI_OS_OK on success; MINI_OS_ERR_INVAL on invalid arguments
 * @note only unlinks the wheel node; the caller decides the next state
 *       (e.g. resume -> add to the ready/running list, delete -> free the TCB)
 */
mini_os_err_t mini_os_remove_thread_from_blocked_list(mini_os_thread_t *thread);

/**
 * @brief Park a thread in the time wheel for 'ticks' ticks (state -> BLOCKED)
 * @param[in] thread thread to park (must not be linked anywhere)
 * @param[in] ticks delay length in ticks (> 0)
 * @return MINI_OS_OK on success; MINI_OS_ERR_INVAL on invalid arguments
 * @note caller must hold interrupts disabled
 */
mini_os_err_t mini_os_wheel_insert(mini_os_thread_t *thread, mini_os_uint32_t ticks);

/**
 * @brief Remaining ticks of a wheel-parked thread
 * @param[in] thread thread to query
 * @return remaining ticks; 0 when the thread is not parked in the wheel
 */
mini_os_uint32_t mini_os_wheel_remain(mini_os_thread_t *thread);

MINI_OS_STATIC_INLINE mini_os_uint8_t mini_os_get_highest_priority(void)
{
    mini_os_uint32_t group = g_priority;

    if (group == 0u)
    {
        return (mini_os_uint8_t)MINI_OS_PRIORITY; /* no ready thread: out-of-range marker */
    }
    return (mini_os_uint8_t)MINI_OS_CTZ(group);
}

#ifdef MINI_OS_LONG_TIME
mini_os_err_t mini_os_get_tick_long_time(mini_os_uint32_t *tick, mini_os_uint32_t *overflow);
#endif

mini_os_err_t mini_os_get_tick(mini_os_uint32_t *tick);

/**
 * @brief Initialize the SysTick timer
 * @param[in] ticks_per_ms Number of ticks per millisecond
 * @note
 *  - void mini_os_systick_init(uint32_t ticks_per_ms)
 */
void mini_os_systick_init(uint32_t ticks_per_ms);

/**
 * @brief SysTick interrupt handler (installed in the vector table)
 */
void mini_os_systick_handler(void);

#ifdef __cplusplus
}
#endif
#endif
