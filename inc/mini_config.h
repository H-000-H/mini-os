/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mini_config.h
 * @brief mini-os configuration header - only include definitions
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

/*
 * @brief Define MINI_NULL_TO_STANDARD to use standard NULL macro instead of 0
 */
#define MINI_NULL_TO_STANDARD

/*
 * @brief Define CONFIG_THREADS_NAME_MAX to set the maximum length of thread names
 */
#ifdef CONFIG_THREADS_NAME_LEN
#define THREADS_NAME_LEN CONFIG_THREADS_NAME_LEN
#else
#define THREADS_NAME_LEN 32
#endif

#ifdef CONFIG_QUEUE_NAME_LEN
#define QUEUE_NAME_LEN CONFIG_QUEUE_NAME_LEN
#else
#define QUEUE_NAME_LEN 32
#endif

#ifdef CONFIG_POOL_NAME_LEN
#define POOL_NAME_LEN CONFIG_POOL_NAME_LEN
#else
#define POOL_NAME_LEN 32
#endif

#ifdef CONFIG_BUFFER_BLOCK_MAX_SIZE
#define BUFFER_BLOCK_MAX_SIZE CONFIG_BUFFER_BLOCK_MAX_SIZE
#else
#define BUFFER_BLOCK_MAX_SIZE 24
#endif

#endif /* MINI_CONFIG_H */
