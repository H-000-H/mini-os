/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file sem.h
 * @brief semaphore implementation
 * @author H-000-H
 */
#ifndef SEM_H
#define SEM_H
#include "err.h"
#if defined(cplusplus)
extern "C" {
#endif
#include "list.h"
#include "redef.h"
#include "queue.h"
typedef struct mini_os_semaphore mini_os_semaphore_t;

/**
 * @brief Semaphore structure
 */
struct mini_os_semaphore
{
    mini_uint16_t   count; /**< Semaphore count */
    mini_bool_t     binary; /**< Binary semaphore flag */
    mini_os_queue_t queue; /**< Semaphore queue */
};

/**
 * @brief Convert a numeric semaphore to a binary semaphore
 * @param[in,out] sem Semaphore to convert
 * @return MINI_OK on success, MINI_ERR_INVAL if sem is NULL
 */
MINI_STATIC_INLINE mini_err_t num_semaphore_to_binary(mini_os_semaphore_t *sem)
{
    if (sem == MINI_NULL)
        return MINI_ERR_INVAL;
    sem->binary = MINI_TRUE;
    sem->count = mini_boolify(sem->count);
    return MINI_OK;
}

#if defined(cplusplus)
}
#endif

#endif /* SEM_H */
