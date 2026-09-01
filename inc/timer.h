/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file timer.h
 * @brief Timer interface
 * @author H-000-H
 * @details For a linked list array consisting of N elements across X groups, the default maximum limit is 32.
 */
#ifndef TIMER_H
#define TIMER_H
#include "list.h"
#include <redef.h>
typedef struct mini_os_timer mini_os_timer_t;

/**
 * @brief Structure representing a mini-os timer
 */
typedef struct mini_os_timer
{
    mini_os_list_t      list_node;          /**< list node for timer */
    char                timer_name[THREADS_NAME_LEN]; /**< timer name */
    void                (*cb)(void *arg);   /**< callback function */
    void                *arg;               /**< callback function argument */
    mini_os_tick_t      init_tick;          /**< user set init tick */
    mini_os_tick_t      timeout_tick;       /**< absolute timeout tick */
    mini_os_uint8_t     flag;               /**< ONE_SHOT/PERIODIC; HARD/SOFT */
    mini_os_bool_t      is_active;          /**< timer is active or not */
} mini_os_timer_t;



#endif
