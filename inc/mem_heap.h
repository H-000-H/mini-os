/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file mem_heap.h
 * @brief Memory heap definition and link lds script
 * @author H-000-H
 */
#ifndef MEM_HEAP_H
#define MEM_HEAP_H
#if defined (cplusplus)
extern "C" {
#endif
#include "redef.h"
extern char __mini_head_start;
extern char __mini_head_end;
#define MINI_HEAP_SIZE (__mini_head_end - __mini_head_start)
MINI_CONSTRUCTOR(100)  mini_err_t mini_os_heap_init(void);
#if defined (cplusplus)
}
#endif
#endif
