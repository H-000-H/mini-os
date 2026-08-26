/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file redef.h
 * @author H-OOO-H
 * @brief mini-os redefinition macros file
 * @details
 *   - defines 'atomic' 'base types' 'gcc/clang differences' 'built-in macros'
 *   - this project defaults to not including standard c library headers and versions v0.1.0 defaults to 32-bit mode
 *   - temporary not included 64-bit equipment
 */
#ifndef REDEF_H
#define REDEF_H

#include <stdint.h>
#if defined (__cplusplus)
extern "C" {
#endif
#include "mini_config.h"

/*---------------------------------------------------------------------------------------------------------*/
/*                                          base type                                                      */
/*---------------------------------------------------------------------------------------------------------*/
typedef signed char                         mini_int8_t;                /*<mini-os int8_t*/
typedef signed short                        mini_int16_t;               /*<mini-os int16_t*/
typedef signed int                          mini_int32_t;               /*<mini-os int32_t*/
typedef signed long long                    mini_int64_t;               /*<mini-os int64_t*/
typedef unsigned char                       mini_uint8_t;               /*<mini-os uint8_t*/
typedef unsigned short                      mini_uint16_t;              /*<mini-os uint16_t*/
typedef unsigned int                        mini_uint32_t;              /*<mini-os uint32_t*/
typedef unsigned long long                  mini_uint64_t;              /*<mini-os uint64_t*/

/*---------------------------------------------------------------------------------------------------------*/
/*                                    volatile Modification base type                                      */
/*---------------------------------------------------------------------------------------------------------*/
typedef volatile signed char                mini_volatile_int8_t;       /*<mini-os volatile int8_t*/
typedef volatile signed short               mini_volatile_int16_t;      /*<mini-os volatile int16_t*/
typedef volatile signed int                 mini_volatile_int32_t;      /*<mini-os volatile int32_t*/
typedef mini_int32_t                        mini_err_t;                 /*<mini-os err_t*/

#define MINI_TRUE                           (1)                         /*<true*/
#define MINI_FALSE                          (0)                         /*<false*/

#ifdef MINI_NULL_TO_STANDARD
#define MINI_NULL                           ((void *)0)                 /*<null*/
#else
#define MINI_NULL                           (0)                         /*<null*/
#endif

#define MINI_UINT8_MAX                      (0XFF)                       /*<uint8_t max*/
#define MINI_UINT16_MAX                     (0XFFFF)                     /*<uint16_t max*/
#define MINI_UINT32_MAX                     (0xFFFFFFFF)                 /*<uint32_t max*/

#if defined(__clang__) || defined(__GNUC__)
#define MINI_CLZ(x)                         (__builtin_clz(x))          /*<count leading zeros*/
#define MINI_CTZ(x)                         (__builtin_ctz(x))          /*<count trailing zeros*/
#define MINI_POPCOUNT(x)                    (__builtin_popcount(x))     /*<count set bits*/
#define MINI_BSWAP(x)                       (__builtin_bswap(x))        /*<byte swap reversal big endian and little endian*/
#define MINI_ALIGN(x)                       __attribute__((aligned(x))) /*<Compile-time alignment */


#endif

#if defined (__cplusplus)
}
#endif

#endif /* REDEF_H */
