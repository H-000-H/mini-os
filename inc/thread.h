/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @brief mini-os thread
 * @file thread.h
 * @author H-000-H
 * @note
 *  - not support multi-core
 */
#ifndef MINI_OS_THREAD_H
#define MINI_OS_THREAD_H
#include "semaphore.h"
#include "timer.h"
#if defined(__cplusplus)
extern "C" {
#endif
#include <redef.h>
#include <list.h>
#include <mini_config.h>

typedef struct mini_os_thread mini_os_thread_t;
/**
 * @brief Structure representing a mini-os thread
 */
typedef enum
{
    MINI_OS_THREAD_STATE_RUNNING=0,             /**< Thread is running */
    MINI_OS_THREAD_STATE_READY,                 /**< Thread is ready to run */
    MINI_OS_THREAD_STATE_SUSPENDED,             /**< Thread is suspended */
    MINI_OS_THREAD_STATE_BLOCKED,               /**< Thread is blocked */
    MINI_OS_THREAD_STATE_INVALID,               /**< Thread is invalid */
    MINI_OS_THREAD_STATE_TERMINATED,            /**< Thread is terminated */
} mini_os_thread_state_t;

struct mini_os_thread
{
    char thread_name[THREADS_NAME_LEN];         /**< Name of the thread */
    mini_os_list_t list_node;                   /**< List node for the thread */
    void (*entry)(void *);                      /**< Entry function for the thread */
    void* param;                                /**< Parameter for the entry function */
    void* sp;                                   /**< Stack pointer for the thread */
    void* stack_addr;                           /**< Stack address for the thread */
    mini_os_uint32_t stack_size;                /**< Stack size for the thread */
    mini_os_thread_state_t state;               /**< State of the thread */
    mini_os_err_t err;                          /**< Error code for the thread */
    mini_os_uint8_t priority;                   /**< Priority of the thread */
    mini_os_tick_t init_tick_num;               /**< Initial tick for the thread for same priority */
    mini_os_tick_t remain_tick;                 /**< Remaining tick for the thread for same priority */
    mini_os_timer_t timer;                      /**< Timer for the thread */
    mini_os_thread_t* self;                     /**< Pointer to this thread structure (self reference) */
    mini_os_user_data_t  user_data;             /**< User data for the thread */
    void (*thread_cleanup)(void *);             /**< Cleanup function for the thread */
#if defined (MINI_OS_THREAD_DETACH)
    mini_os_bool_t      is_detach;              /**< thread detach flag */
    mini_os_bool_t      is_terminated;          /**< thread terminated flag */
    void                *exit_retval;           /**< thread exit return value for join */
    mini_os_semaphore_t *join_wait_sem;         /**< semaphore for join block wait */
#endif
};

/**
 * @brief Create a thread
 * @param[in] name Name of the thread
 * @param[in] stack_size Stack size for the thread
 * @param[in] priority Priority of the thread
 * @param[in] entry Entry function for the thread
 * @param[in] param Parameter for the entry function
 * @return mini_os_thread_t* on success, MINI_OS_NULL on failure
 */
mini_os_thread_t* mini_os_thread_create(                            const char* name,
                                                                    mini_os_uint32_t stack_size,
                                                                    mini_os_uint8_t priority,
                                                                    void (*entry)(void *),
                                                                    void* param);

/**
 * @brief Delete a thread
 * @param[in] thread Thread to delete
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_delete(                                mini_os_thread_t* thread);

/**
 * @brief Create a thread statically
 * @param[in] name Name of the thread
 * @param[in] stack_size Stack size for the thread
 * @param[in] priority Priority of the thread
 * @param[in] entry Entry function for the thread
 * @param[in] param Parameter for the entry function
 * @param[in] stack_buffer Stack buffer for the thread
 * @param[in] task_buffer Thread control block storage (mini_os_thread_t)
 * @return mini_os_thread_t* on success, MINI_OS_NULL on failure
 */
mini_os_thread_t* mini_os_thread_create_static(                     const char* name,
                                                                    mini_os_uint32_t stack_size,
                                                                    mini_os_uint8_t priority,
                                                                    void (*entry)(void *),
                                                                    void* param,
                                                                    mini_os_uint32_t* stack_buffer,
                                                                    mini_os_thread_t* task_buffer);

/**
 * @brief Delete a thread statically
 * @param[in] thread Thread to delete
 * @param[in] stack_buffer Stack buffer for the thread
 * @param[in] task_buffer Task buffer for the thread
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_delete_static(                         mini_os_thread_t* thread,
                                                                    mini_os_uint32_t* stack_buffer,
                                                                    mini_os_thread_t* task_buffer);


/**
 * @brief Find a thread by name
 * @param[in] name Name of the thread
 * @return mini_os_thread_t* on success, MINI_OS_NULL on failure
 */
mini_os_thread_t* mini_os_find_by_name(                             const char* name);

/**
 * @brief Yield the CPU to the scheduler
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_yield(                                 void);

/**
 * @brief Start a thread
 * @param[in] thread Thread to start
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_start(                                 mini_os_thread_t* thread);

/**
 * @brief Suspend a thread
 * @param[in] thread Thread to suspend
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_suspend(                               mini_os_thread_t* thread);

/**
 * @brief Resume a thread
 * @param[in] thread Thread to resume
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_resume(                                mini_os_thread_t* thread);


/**
 * @brief Get the current thread
 * @return mini_os_thread_t* on success, MINI_OS_NULL on failure
 */
mini_os_thread_t* mini_os_thread_current(                           void);

/**
 * @brief Delay for a specified number of ticks
 * @param[in] ticks Number of ticks to delay
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_delay_tick(                            mini_os_uint32_t ticks);

/**
 * @brief Delay for a specified number of milliseconds
 * @param[in] ms Number of milliseconds to delay
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_delay_ms(                              mini_os_uint32_t ms);

/**
 * @brief Delay until a specified ticks
 * @param[in] ticks Number of ticks to delay until
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_delay_tick_until(                      mini_os_uint32_t ticks);

/**
 * @brief Set the name of a thread
 * @param[in] thread Thread to set the name for
 * @param[in] name Name to set
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_set_name(                              mini_os_thread_t* thread, const char* name);

/**
 * @brief Get the name of a thread
 * @param[in] thread Thread to get the name for
 * @param[out] name Buffer to store the name in
 * @param[in] name_len Length of the name buffer
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_get_name(                              mini_os_thread_t* thread, char* name, mini_os_uint32_t* name_len);

/**
 * @brief Set the priority of a thread
 * @param[in] thread Thread to set the priority for
 * @param[in] priority Priority to set
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_set_priority(                          mini_os_thread_t* thread, mini_os_uint8_t priority);

/**
 * @brief Get the priority of a thread
 * @param[in] thread Thread to get the priority for
 * @param[out] priority Buffer to store the priority in
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_get_priority(                          mini_os_thread_t* thread, mini_os_uint8_t* priority);

/**
 * @brief Set the timeslice of a thread
 * @param[in] thread Thread to set the timeslice for
 * @param[in] tick Timeslice to set
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_set_timeslice(mini_os_thread_t* thread, mini_os_tick_t tick);

/**
 * @brief Get the timeslice of a thread
 * @param[in] thread Thread to get the timeslice for
 * @param[out] tick Buffer to store the timeslice in
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_get_timeslice(mini_os_thread_t* thread, mini_os_tick_t* tick);

/**
 * @brief Get the state of a thread
 * @param[in] thread Thread to get the state for
 * @param[out] state Buffer to store the state in
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_get_state(mini_os_thread_t* thread, mini_os_thread_state_t *state);

/**
 * @brief Set the user data of a thread
 * @param[in] thread Thread to set the user data for
 * @param[in] user_data User data to set
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_set_user_data(mini_os_thread_t* thread, mini_os_user_data_t user_data);

/**
 * @brief Get the user data of a thread
 * @param[in] thread Thread to get the user data for
 * @param[out] user_data Buffer to store the user data in
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_get_user_data(mini_os_thread_t* thread, mini_os_user_data_t* user_data);

/**
 * @brief Set the cleanup function of a thread
 * @param[in] thread Thread to set the cleanup function for
 * @param[in] cleanup Cleanup function to set
 * @param[in] arg Argument to pass to the cleanup function
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_set_cleanup(mini_os_thread_t* thread, void (*cleanup)(void *), void *arg);

#if defined(MINI_OS_THREAD_DETACH)
/**
 * @brief Detach a thread
 * @param[in] thread Thread to detach
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_detach(mini_os_thread_t* thread);

/**
 * @brief Join a thread
 * @param[in] thread Thread to join
 * @param[out] thread_return Buffer to store the thread return value in
 * @param[in] timeout_tick Timeout tick for the join operation
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_join(mini_os_thread_t* thread, void **thread_return, mini_os_tick_t timeout_tick);
#endif /* MINI_OS_THREAD_DETACH */

/**
 * @brief Initialize the idle thread
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_idle_init(void);

/**
 * @brief Get the idle thread handle
 * @return mini_os_thread_t* on success, MINI_OS_NULL on failure
 */
mini_os_thread_t* mini_os_thread_get_idle_handle(void);

typedef void (*idle_hook_t)(void *);
/**
 * @brief Set the idle hook for a thread
 * @param[in] hook Idle hook function
 * @param[in] param Argument passed to the idle hook
 * @return mini_os_err_t on success, 0 on failure
 */
mini_os_err_t mini_os_thread_idle_hook(idle_hook_t hook, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* MINI_OS_THREAD_H */
