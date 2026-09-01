/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @brief queue implementation
 * @file queue.c
 * @author H-000-H
 */
#include "queue.h"
#include "err.h"
#include "memory.h"
#include "redef.h"
#include "schedule.h"

/**
 * @brief Copy a name into the fixed-size name field (always NUL-terminated)
 */
static void mini_os_queue_name_set(char* dst, const char* name)
{
    mini_os_size_t i = 0;

    if (name != MINI_OS_NULL)
    {
        while (name[i] != '\0' && i < QUEUE_NAME_LEN - 1)
        {
            dst[i] = name[i];
            i++;
        }
    }
    dst[i] = '\0';
}

/**
 * @brief Reverse-lookup the slot address of a slot index (heap + idx * msg_size)
 */
static mini_os_uint8_t* mini_os_queue_slot_at(mini_os_queue_t* queue, mini_os_uint8_t idx)
{
    return (mini_os_uint8_t*)queue->msg_base + (mini_os_size_t)idx * queue->msg_size;
}

/**
 * @brief Initialize a queue descriptor over an already-provided message pool
 */
static mini_os_err_t mini_os_queue_init(mini_os_queue_t* queue,
                                        const char* name,
                                        mini_os_uint16_t msg_size,
                                        mini_os_uint8_t depth,
                                        void* msg_base,
                                        mini_os_bool_t heap_owned)
{
    mini_os_queue_name_set(queue->name, name);
    queue->msg_size = msg_size;
    queue->max_depth = depth;
    queue->depth = 0;
    queue->write_idx = 0;
    queue->read_idx = 0;
    queue->heap_owned = heap_owned;
    queue->msg_base = msg_base;
    mini_os_list_init(&queue->send_list);
    mini_os_list_init(&queue->receive_list);
    return MINI_OS_OK;
}

/**
 * @brief Wake the oldest waiter of a wait list (caller holds interrupts disabled)
 * @return MINI_OS_TRUE when a thread was moved back to the ready list
 */
static mini_os_bool_t mini_os_queue_wake_one(mini_os_list_t* wait_list)
{
    mini_os_thread_t* thread;

    if (mini_os_list_is_empty(wait_list))
    {
        return MINI_OS_FALSE;
    }
    thread = mini_os_container_of(wait_list->next, mini_os_thread_t, wait_node);
    mini_os_list_remove(&thread->wait_node);
    thread->wait_list = MINI_OS_NULL;
    thread->wait_done = MINI_OS_TRUE;
    if (thread->wheel_slot < MINI_OS_TICK_WHEEL)
    {
        /* timed sync wait: the thread is also parked in the time wheel */
        (void)mini_os_remove_thread_from_blocked_list(thread);
    }
    (void)mini_os_add_thread_to_ready_running_list(thread);
    return MINI_OS_TRUE;
}

mini_os_queue_t* mini_os_queue_create(const char* name,
                                      mini_os_uint16_t msg_size,
                                      mini_os_uint8_t depth)
{
    mini_os_queue_t* queue;
    void* pool;

    if (msg_size == 0 || depth == 0)
    {
        return MINI_OS_NULL;
    }

    queue = (mini_os_queue_t*)mini_os_malloc(sizeof(mini_os_queue_t));
    if (queue == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }

    pool = mini_os_malloc((mini_os_size_t)depth * msg_size);
    if (pool == MINI_OS_NULL)
    {
        mini_os_free(queue);
        return MINI_OS_NULL;
    }

    mini_os_queue_init(queue, name, msg_size, depth, pool, MINI_OS_TRUE);
    return queue;
}

mini_os_queue_t* mini_os_queue_create_static(const char* name,
                                             mini_os_uint16_t msg_size,
                                             mini_os_uint8_t depth,
                                             mini_os_queue_t* queue_buffer,
                                             void* msg_buffer,
                                             mini_os_size_t buffer_size)
{
    if (msg_size == 0 || depth == 0 || queue_buffer == MINI_OS_NULL || msg_buffer == MINI_OS_NULL ||
        buffer_size < (mini_os_size_t)depth * msg_size)
    {
        return MINI_OS_NULL;
    }

    mini_os_queue_init(queue_buffer, name, msg_size, depth, msg_buffer, MINI_OS_FALSE);
    return queue_buffer;
}

mini_os_err_t mini_os_queue_delete(mini_os_queue_t* queue)
{
    mini_os_irq_t irq;

    if (queue == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (!queue->heap_owned)
    {
        return MINI_OS_ERR_NOTSUPP;
    }

    irq = mini_os_irq_save();
    if (!mini_os_list_is_empty(&queue->send_list) || !mini_os_list_is_empty(&queue->receive_list))
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;
    }
    mini_os_irq_restore(irq);

    mini_os_free(queue->msg_base);
    mini_os_free(queue);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_queue_send(mini_os_queue_t* queue, const void* msg, mini_os_tick_t timeout_tick)
{
    mini_os_bool_t woken;
    mini_os_uint32_t deadline = 0;

    if (queue == MINI_OS_NULL || msg == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (timeout_tick > 0 && timeout_tick != MINI_OS_QUEUE_WAIT_FOREVER)
    {
        mini_os_uint32_t now = 0;

        (void)mini_os_get_tick(&now);
        deadline = now + (mini_os_uint32_t)timeout_tick; /* strict total timeout */
    }

    for (;;)
    {
        mini_os_irq_t irq = mini_os_irq_save();

        if (queue->depth < queue->max_depth)
        {
            MINI_OS_MEMCPY(mini_os_queue_slot_at(queue, queue->write_idx), msg, queue->msg_size);
            queue->write_idx++;
            if (queue->write_idx >= queue->max_depth)
            {
                queue->write_idx = 0;
            }
            queue->depth++;
            woken = mini_os_queue_wake_one(&queue->receive_list);
            mini_os_irq_restore(irq);
            if (woken)
            {
                (void)mini_os_schedule_yield();
            }
            return MINI_OS_OK;
        }

        if (timeout_tick == 0)
        {
            mini_os_irq_restore(irq);
            return MINI_OS_ERR_AGAIN;
        }
        if (mini_os_thread_current() == MINI_OS_NULL)
        {
            mini_os_irq_restore(irq);
            return MINI_OS_ERR_INVAL; /* no thread context: only timeout 0 is supported */
        }
        /* Park on the wait list (wait_node) and, for a finite timeout, also in
         * the time wheel (list_node), inside the critical section so a space
         * event cannot be missed; park restores irq and yields. A retry parks
         * with the remaining time only, keeping the total timeout strict. */
        if (timeout_tick != MINI_OS_QUEUE_WAIT_FOREVER)
        {
            mini_os_uint32_t remain = mini_os_tick_until(deadline);

            if (remain == 0u)
            {
                mini_os_irq_restore(irq);
                return MINI_OS_ERR_TIMEOUT;
            }
            if (mini_os_sync_wait_park(&queue->send_list, 0u, (mini_os_tick_t)remain, irq) != MINI_OS_OK)
            {
                return MINI_OS_ERR_TIMEOUT;
            }
        }
        else if (mini_os_sync_wait_park(&queue->send_list, 0u, timeout_tick, irq) != MINI_OS_OK)
        {
            return MINI_OS_ERR_TIMEOUT;
        }
    }
}

mini_os_err_t mini_os_queue_receive(mini_os_queue_t* queue, void* msg, mini_os_tick_t timeout_tick)
{
    mini_os_bool_t woken;
    mini_os_uint32_t deadline = 0;

    if (queue == MINI_OS_NULL || msg == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (timeout_tick > 0 && timeout_tick != MINI_OS_QUEUE_WAIT_FOREVER)
    {
        mini_os_uint32_t now = 0;

        (void)mini_os_get_tick(&now);
        deadline = now + (mini_os_uint32_t)timeout_tick; /* strict total timeout */
    }

    for (;;)
    {
        mini_os_irq_t irq = mini_os_irq_save();

        if (queue->depth > 0)
        {
            MINI_OS_MEMCPY(msg, mini_os_queue_slot_at(queue, queue->read_idx), queue->msg_size);
            queue->read_idx++;
            if (queue->read_idx >= queue->max_depth)
            {
                queue->read_idx = 0;
            }
            queue->depth--;
            woken = mini_os_queue_wake_one(&queue->send_list);
            mini_os_irq_restore(irq);
            if (woken)
            {
                (void)mini_os_schedule_yield();
            }
            return MINI_OS_OK;
        }

        if (timeout_tick == 0)
        {
            mini_os_irq_restore(irq);
            return MINI_OS_ERR_AGAIN;
        }
        if (mini_os_thread_current() == MINI_OS_NULL)
        {
            mini_os_irq_restore(irq);
            return MINI_OS_ERR_INVAL; /* no thread context: only timeout 0 is supported */
        }
        /* Park on the wait list (wait_node) and, for a finite timeout, also in
         * the time wheel (list_node), inside the critical section so a message
         * event cannot be missed; park restores irq and yields. A retry parks
         * with the remaining time only, keeping the total timeout strict. */
        if (timeout_tick != MINI_OS_QUEUE_WAIT_FOREVER)
        {
            mini_os_uint32_t remain = mini_os_tick_until(deadline);

            if (remain == 0u)
            {
                mini_os_irq_restore(irq);
                return MINI_OS_ERR_TIMEOUT;
            }
            if (mini_os_sync_wait_park(&queue->receive_list, 0u, (mini_os_tick_t)remain, irq) != MINI_OS_OK)
            {
                return MINI_OS_ERR_TIMEOUT;
            }
        }
        else if (mini_os_sync_wait_park(&queue->receive_list, 0u, timeout_tick, irq) != MINI_OS_OK)
        {
            return MINI_OS_ERR_TIMEOUT;
        }
    }
}

mini_os_err_t mini_os_queue_send_isr(mini_os_queue_t* queue, const void* msg)
{
    mini_os_irq_t irq;

    if (queue == MINI_OS_NULL || msg == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    if (queue->depth >= queue->max_depth)
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_AGAIN; /* ISR path = timeout 0 path: never block */
    }
    MINI_OS_MEMCPY(mini_os_queue_slot_at(queue, queue->write_idx), msg, queue->msg_size);
    queue->write_idx++;
    if (queue->write_idx >= queue->max_depth)
    {
        queue->write_idx = 0;
    }
    queue->depth++;
    /* Wake only; the ISR caller decides via
     * mini_os_queue_isr_is_heigher_priority() whether to trigger PendSV. */
    (void)mini_os_queue_wake_one(&queue->receive_list);
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_queue_receive_isr(mini_os_queue_t* queue, void* msg)
{
    mini_os_irq_t irq;

    if (queue == MINI_OS_NULL || msg == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    if (queue->depth == 0)
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_AGAIN; /* ISR path = timeout 0 path: never block */
    }
    MINI_OS_MEMCPY(msg, mini_os_queue_slot_at(queue, queue->read_idx), queue->msg_size);
    queue->read_idx++;
    if (queue->read_idx >= queue->max_depth)
    {
        queue->read_idx = 0;
    }
    queue->depth--;
    (void)mini_os_queue_wake_one(&queue->send_list);
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_queue_isr_is_heigher_priority(mini_os_bool_t* is_heigher_priority)
{
    mini_os_thread_t* current;
    mini_os_uint8_t highest;
    mini_os_irq_t irq;

    if (is_heigher_priority == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    *is_heigher_priority = MINI_OS_FALSE;

    irq = mini_os_irq_save();
    current = mini_os_thread_current();
    if (current != MINI_OS_NULL)
    {
        highest = mini_os_get_highest_priority();
        if (highest < current->priority)
        {
            *is_heigher_priority = MINI_OS_TRUE;
        }
    }
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_bool_t mini_os_queue_is_empty(mini_os_queue_t* queue)
{
    if (queue == MINI_OS_NULL)
    {
        return MINI_OS_FALSE;
    }
    return (queue->depth == 0) ? MINI_OS_TRUE : MINI_OS_FALSE;
}

mini_os_bool_t mini_os_queue_is_full(mini_os_queue_t* queue)
{
    if (queue == MINI_OS_NULL)
    {
        return MINI_OS_FALSE;
    }
    return (queue->depth == queue->max_depth) ? MINI_OS_TRUE : MINI_OS_FALSE;
}

mini_os_uint8_t mini_os_queue_get_depth(mini_os_queue_t* queue)
{
    if (queue == MINI_OS_NULL)
    {
        return 0;
    }
    return queue->depth;
}
