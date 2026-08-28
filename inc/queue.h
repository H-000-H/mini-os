/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file queue.h
 * @brief queue implementation
 * @author H-000-H
 */
#ifndef QUEUE_H
#define QUEUE_H
#if defined(cplusplus)
extern "C" {
#endif
#include "list.h"
#include "redef.h"
#include "mini_config.h"
typedef struct mini_os_queue mini_os_queue_t;
struct mini_os_queue
{
    char name[QUEUE_NAME_LEN]; /**< queue name */
    mini_os_uint16_t msg_size; /**< queue message size */
    mini_os_uint8_t max_depth; /**< queue max depth (the maximum number of messages)*/
    mini_os_uint8_t depth; /**< queue depth (the number of messages in the queue)*/
    void* user_data; /**< user data */
    void* msg_heap; /**< message heap */
    void* msg_tail; /**< message tail */
    mini_os_list_t msg_list; /**< message list */
};
#if defined(cplusplus)
}
#endif

#endif /* QUEUE_H */
