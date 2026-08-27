/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @brief mini-os thread
 * @file thread.h
 * @author H-000-H
 */
#ifndef MINI_OS_THREAD_H
#define MINI_OS_THREAD_H
#include "timer.h"
#if defined(cplusplus)
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
    MINI_OS_THREAD_STATE_RUNNING=0,           /**< Thread is running */
    MINI_OS_THREAD_STATE_READY,             /**< Thread is ready to run */
    MINI_OS_THREAD_STATE_SUSPENDED,         /**< Thread is suspended */
    MINI_OS_THREAD_STATE_BLOCKED,           /**< Thread is blocked */
    MINI_OS_THREAD_STATE_INVALID,           /**< Thread is invalid */
    MINI_OS_THREAD_STATE_TERMINATED,        /**< Thread is terminated */
} mini_os_thread_state_t;

struct mini_os_thread
{
    char thread_name[THREADS_NAME_LEN];     /**< Name of the thread */
    mini_os_list_t list_node;               /**< List node for the thread */
    void (*entry)(void *);                  /**< Entry function for the thread */
    void* param;                            /**< Parameter for the entry function */
    void* sp;                               /**< Stack pointer for the thread */
    void* stack_addr;                       /**< Stack address for the thread */
    mini_uint32_t stack_size;               /**< Stack size for the thread */
    mini_os_thread_state_t state;           /**< State of the thread */
    mini_err_t err;                         /**< Error code for the thread */
    mini_uint8_t priority;                  /**< Priority of the thread */
    mini_tick_t init_tick_num;              /**< Initial tick for the thread for same priority */
    mini_tick_t remain_tick;                /**< Remaining tick for the thread for same priority */
    mini_os_timer_t timer;                  /**< Timer for the thread */
    mini_os_thread_t* this;                 /**< Pointer to the thread structure using to check is same thread */
    mini_user_data_t  user_data;            /**< User data for the thread */
    void (*thread_cleanup)(void *);         /**< Cleanup function for the thread */
};
#if defined(cplusplus)
}
#endif
#endif /* MINI_OS_THREAD_H */
