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
struct mini_os_timer
{
    mini_os_tick_t out_tick;                        /**< deadline tick for the timer */
    mini_os_tick_t time_out;                        /**< Tick count for the timer out tick */
    void (*callback)(void*);                    /**< callback function for the timer */
    void* param;                                 /**< parameter for the callback function */
    mini_os_timer_t* node;                       /**< next timer in the list */
};
#endif
