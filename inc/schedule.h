/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file schedule.h
 * @brief Scheduling functions
 * @author H-000-H
 */
#ifndef SCHEDULE_H
#define SCHEDULE_H
#ifdef __cplusplus
extern "C" {
#endif
#include "redef.h"
#include "thread.h"
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
 * @brief Entry function for scheduling request
 * @return 0 on success, negative error code on failure
 * @note only used by the kernel user cannot call this function directly
 */
mini_os_err_t mini_os_schedule_kernel(void);
/**
 * @brief Add a thread to the ready queue
 * @param[in] thread The thread to add
 * @return 0 on success, negative error code on failure
 */
mini_os_err_t mini_os_add_thread_to_ready_queue(mini_os_thread_t *thread);
/**
 * @brief Remove a thread from the ready queue
 * @param[in] thread The thread to remove
 * @return 0 on success, negative error code on failure
 */
mini_os_err_t mini_os_remove_thread_from_ready_queue(mini_os_thread_t *thread);

#ifdef __cplusplus
}
#endif
#endif
