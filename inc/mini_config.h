/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mini_config.h
 * @brief mini-os configuration heaper - only include definitions
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
 * @brief Define MINI_OS_NULL_TO_STANDARD to use standard NULL macro instead of 0
 */
#define MINI_OS_NULL_TO_STANDARD

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

#define MINI_OS_IDLE_THREAD_NAME "idle_thread"

#define MINI_OS_IDLE_THREAD_PRIORITY 1

#ifdef CONFIG_MINI_IDLE_THREAD_STACK_SIZE
#define MINI_OS_IDLE_THREAD_STACK_SIZE CONFIG_MINI_IDLE_THREAD_STACK_SIZE
#else
#define MINI_OS_IDLE_THREAD_STACK_SIZE 512
#endif

#define MINI_OS_IDLE_THREAD_CONSTRUCTOR 105

#ifdef CONFIG_MINI_EXECUTION_SLAB_CHECK_SIZE
#define MINI_EXECUTION_SLAB_CHECK_SIZE CONFIG_MINI_EXECUTION_SLAB_CHECK_SIZE
#else
#define MINI_EXECUTION_SLAB_CHECK_SIZE 0
#endif

/*
 * @brief Enable thread detach/join support (adds per-thread join fields)
 * @note Off by default; define CONFIG_MINI_OS_THREAD_DETACH in config.h to enable
 */
#ifdef CONFIG_MINI_OS_THREAD_DETACH
#define MINI_OS_THREAD_DETACH
#endif
#endif /* MINI_CONFIG_H */
