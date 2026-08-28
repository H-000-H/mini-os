/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file err.h
 * @brief Error codes
 * @author H-000-H
 */
#ifndef ERR_H
#define ERR_H
/**
 * @brief Error codes if config.h and compiler_compat.h are available using status.h otherwise using self defined error codes
 * @note numbers are negative to distinguish from success codes and self error codes different from status.h
 */
#include "redef.h"
#if __has_include(<config.h>) && __has_include(<compiler_compat.h>)
#include <status.h>
#else
#define MINI_OS_OK 0
#define MINI_OS_ERR_INVAL -1       /**<invalid parameter */
#define MINI_OS_ERR_ISR -2         /**<interrupt context illegal call */
#define MINI_OS_ERR_NOMEM -3       /**<memory insufficient */
#define MINI_OS_ERR_IO -4          /**<physical IO error */
#define MINI_OS_ERR_BUSY -5        /**<device busy */
#define MINI_OS_ERR_AGAIN -6       /**<retry */
#define MINI_OS_ERR_NOSPC -7       /**<no remaining space/channel */
#define MINI_OS_ERR_TIMEOUT -8     /**<lock acquisition/operation timeout */
#define MINI_OS_ERR_HW_FATAL -9    /**<hardware physical fault, unrecoverable */
#define MINI_OS_ERR_DEFER -10      /**<dependency not ready, retry later */
#define MINI_OS_ERR_NODEV -11      /**<device removed or not exist */
#define MINI_OS_ERR_NOTSUPP -12    /**<operation not supported/implemented */

#endif

#endif
