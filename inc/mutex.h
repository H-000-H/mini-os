/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mutex.h
 * @brief mutex implementation
 * @author H-000-H
 */
#ifndef MUTEX_H
#define MUTEX_H
#include "redef.h"
#include "sem.h"
#include "thread.h"
#if defined(cplusplus)
extern "C" {
#endif

typedef struct mini_os_mutex mini_os_mutex_t;
/**
 * @brief Mutex structure
 */
struct mini_os_mutex
{
    mini_os_semaphore_t semaphore; /**< mutex inheritance through Semaphore */
    mini_uint8_t depth; /**< Mutex depth */
    mini_uint8_t priority; /**< Mutex priority */
    mini_os_thread_t *owner; /**< Mutex owner */
};
#if defined(cplusplus)
}
#endif

#endif /* MUTEX_H */
