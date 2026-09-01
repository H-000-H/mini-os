/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @brief Event group implementation (32 event flags, OR/WHOLE wait semantics,
 *        blocking wait with wait-list wake-up and time-wheel timeout)
 * @file event.c
 * @author H-000-H
 */
#include "event.h"
#include "err.h"
#include "list.h"
#include "redef.h"
#include "schedule.h"
#include "thread.h"

mini_os_event_group_t* mini_os_event_group_create(mini_os_uint32_t event_id,mini_os_event_type_t type)
{
    mini_os_event_group_t* event_group = (mini_os_event_group_t*)mini_os_malloc(sizeof(mini_os_event_group_t));

    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    mini_os_list_init(&event_group->wait_list);
    event_group->event = event_id;
    event_group->event_type = type;
    event_group->is_auto_clear = MINI_OS_TRUE;
    event_group->heap_owned = MINI_OS_TRUE;
    return event_group;
}

mini_os_event_group_t* mini_os_event_group_create_static(mini_os_event_group_t* event_group,mini_os_uint32_t event_id,mini_os_event_type_t type)
{
    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    mini_os_list_init(&event_group->wait_list);
    event_group->event = event_id;
    event_group->event_type = type;
    event_group->is_auto_clear = MINI_OS_TRUE;
    event_group->heap_owned = MINI_OS_FALSE;
    return event_group;
}

mini_os_err_t mini_os_event_group_delete(mini_os_event_group_t* event_group)
{
    mini_os_irq_t irq;

    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (!event_group->heap_owned)
    {
        return MINI_OS_ERR_NOTSUPP;
    }

    irq = mini_os_irq_save();
    if (!mini_os_list_is_empty(&event_group->wait_list))
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;
    }
    mini_os_irq_restore(irq);

    mini_os_free(event_group);
    return MINI_OS_OK;
}

/**
 * @brief Check whether the current flags satisfy a waiter's mask
 */
static mini_os_bool_t mini_os_event_satisfied(const mini_os_event_group_t* event_group, mini_os_uint32_t mask)
{
    if (event_group->event_type == MINI_OS_EVENT_WHOLE_TYPE)
    {
        return ((event_group->event & mask) == mask) ? MINI_OS_TRUE : MINI_OS_FALSE;
    }
    return ((event_group->event & mask) != 0u) ? MINI_OS_TRUE : MINI_OS_FALSE;
}

/**
 * @brief Wake every waiter whose mask is satisfied (caller holds interrupts disabled)
 * @return MINI_OS_TRUE when at least one thread was moved back to the ready list
 * @param[in] event_group tcp
 * @note bits are NOT consumed here even with is_auto_clear: each woken waiter
 *       re-checks the condition and consumes its own bits in mini_os_event_wait,
 *       so a losing waiter can simply park again instead of missing the event
 */
static mini_os_bool_t mini_os_event_wake_satisfied(mini_os_event_group_t* event_group)
{
    mini_os_list_t* node;
    mini_os_list_t* next;
    mini_os_thread_t* thread;
    mini_os_bool_t woken = MINI_OS_FALSE;

    for (node = event_group->wait_list.next; node != &event_group->wait_list; node = next)
    {
        next = node->next;
        thread = mini_os_container_of(node, mini_os_thread_t, wait_node);
        if (!mini_os_event_satisfied(event_group, thread->wait_mask))
        {
            continue;
        }
        mini_os_list_remove(&thread->wait_node);
        thread->wait_list = MINI_OS_NULL;
        thread->wait_done = MINI_OS_TRUE;
        if (thread->wheel_slot < MINI_OS_TICK_WHEEL)
        {
            /* timed wait: the thread is also parked in the time wheel */
            (void)mini_os_remove_thread_from_blocked_list(thread);
        }
        (void)mini_os_add_thread_to_ready_running_list(thread);
        woken = MINI_OS_TRUE;
    }
    return woken;
}

/**
 * @brief Update the flags and wake satisfied waiters (IRQ-safe)
 * @return MINI_OS_TRUE when at least one waiter was moved back to ready
 */
static mini_os_bool_t mini_os_event_apply(mini_os_event_group_t* event_group, mini_os_uint32_t event)
{
    mini_os_irq_t irq;
    mini_os_bool_t woken;

    irq = mini_os_irq_save();
    if (event_group->event_type == MINI_OS_EVENT_WHOLE_TYPE)
    {
        event_group->event = event; /* WHOLE: the flags are replaced */
    }
    else
    {
        event_group->event |= event;
    }
    woken = mini_os_event_wake_satisfied(event_group);
    mini_os_irq_restore(irq);
    return woken;
}

mini_os_err_t mini_os_event_set_group(mini_os_event_group_t* event_group, mini_os_uint32_t event)
{
    mini_os_bool_t woken;

    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    woken = mini_os_event_apply(event_group, event);
    if (woken)
    {
        (void)mini_os_schedule_yield();
    }
    return MINI_OS_OK;
}

mini_os_err_t mini_os_event_set_group_isr(mini_os_event_group_t* event_group, mini_os_uint32_t event)
{
    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    /* Wake only; the ISR caller decides via
     * mini_os_queue_isr_is_heigher_priority() whether to trigger PendSV. */
    (void)mini_os_event_apply(event_group, event);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_event_get_group(mini_os_event_group_t *event_group, mini_os_uint32_t *event)
{
    mini_os_irq_t irq;

    if (event_group == MINI_OS_NULL || event == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    irq = mini_os_irq_save();
    *event = event_group->event;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_event_clear_group(mini_os_event_group_t* event_group, mini_os_uint32_t event)
{
    mini_os_irq_t irq;

    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    irq = mini_os_irq_save();
    event_group->event &= ~event;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_event_group_set_auto_clear(mini_os_event_group_t* event_group, mini_os_bool_t is_auto_clear)
{
    mini_os_irq_t irq;

    if (event_group == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    irq = mini_os_irq_save();
    event_group->is_auto_clear = is_auto_clear;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_event_wait(mini_os_event_group_t* event_group, mini_os_uint32_t mask, mini_os_tick_t timeout_tick, mini_os_uint32_t* out_event)
{
    mini_os_uint32_t deadline = 0;

    if (event_group == MINI_OS_NULL || mask == 0u)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (timeout_tick > 0 && timeout_tick != (mini_os_tick_t)-1)
    {
        mini_os_uint32_t now = 0;

        (void)mini_os_get_tick(&now);
        deadline = now + (mini_os_uint32_t)timeout_tick; /* strict total timeout */
    }

    for (;;)
    {
        mini_os_irq_t irq = mini_os_irq_save();

        if (mini_os_event_satisfied(event_group, mask))
        {
            if (out_event != MINI_OS_NULL)
            {
                *out_event = event_group->event & mask; /* report before auto-clear consumes */
            }
            if (event_group->is_auto_clear)
            {
                event_group->event &= ~mask; /* consume the retrieved bits */
            }
            mini_os_irq_restore(irq);
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
         * the time wheel (list_node), inside the critical section so a set
         * event cannot be missed; park restores irq and yields. A retry parks
         * with the remaining time only, keeping the total timeout strict. */
        if (timeout_tick != (mini_os_tick_t)-1)
        {
            mini_os_uint32_t remain = mini_os_tick_until(deadline);

            if (remain == 0u)
            {
                mini_os_irq_restore(irq);
                return MINI_OS_ERR_TIMEOUT;
            }
            if (mini_os_sync_wait_park(&event_group->wait_list, mask, (mini_os_tick_t)remain, irq) != MINI_OS_OK)
            {
                return MINI_OS_ERR_TIMEOUT;
            }
        }
        else if (mini_os_sync_wait_park(&event_group->wait_list, mask, timeout_tick, irq) != MINI_OS_OK)
        {
            return MINI_OS_ERR_TIMEOUT;
        }
    }
}
