/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @author H-000-H
 * @file thread.c
 * @brief Thread management functions
 */
#include "thread.h"
#include "redef.h"
static mini_os_err_t mini_os_thread_init(              const char* name,
                                                            mini_os_uint32_t stack_size,
                                                            mini_os_uint8_t priority,
                                                            void (*entry)(void *),
                                                            void* param,
                                                            mini_os_uint32_t* stack_buffer,
                                                            mini_os_thread_t* task_buffer,
                                                            mini_os_tick_t init_tick_num,
                                                            mini_os_thread_t* thread)
{

}
