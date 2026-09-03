/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @brief semaphore implementation
 * @file semaphore.c
 * @author H-000-H
 * @note
 */
#include "semaphore.h"
#include "err.h"
#include "list.h"
#include "memory.h"
#include "mini_config.h"
#include "redef.h"
#include "schedule.h"
#include "thread.h"

#if MINI_OS_FIND_BY_NAME
static mini_os_list_t g_semaphore_list;
MINI_OS_CONSTRUCTOR(MINI_OS_SEMAPHORE_REGISTRY_CONSTRUCTOR) void mini_os_semaphore_registry_init(void)
{
    mini_os_list_init(&g_semaphore_list);
}
#endif

/**
 * @brief Copy a name into the fixed-size name field (always NUL-terminated)
 */
static void mini_os_semaphore_name_set(char* dst, const char* name)
{
    mini_os_size_t i = 0;

    if (name != MINI_OS_NULL)
    {
        while (name[i] != '\0' && i < MINI_OS_SEMAPHORE_NAME_LEN - 1)
        {
            dst[i] = name[i];
            i++;
        }
    }
    dst[i] = '\0';
}

/**
 * @brief Wake the oldest waiter of a semaphore (caller holds interrupts disabled)
 * @return MINI_OS_TRUE when a thread was moved back to the ready list
 */
static mini_os_bool_t mini_os_semaphore_wake_one(mini_os_semaphore_t* semaphore)
{
    mini_os_thread_t* thread;

    if (mini_os_list_is_empty(&semaphore->wait_list))
    {
        return MINI_OS_FALSE;
    }
    thread = mini_os_container_of(semaphore->wait_list.next, mini_os_thread_t, wait_node);
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

/**
 * @brief Initialize a semaphore descriptor over already-provided storage
 * @param[in,out] semaphore semaphore to initialize
 * @param[in] max_count capacity of the semaphore (>= 1; 1 makes it a binary semaphore)
 * @param[in] count initial count (0 = unavailable at start, must be <= max_count)
 * @param[in] is_static MINI_OS_TRUE when the storage is caller-provided (not heap owned)
 * @param[in] name optional name, copied and NUL-terminated (MINI_OS_NULL = empty name)
 * @return MINI_OS_OK on success, MINI_OS_ERR_INVAL on a NULL semaphore,
 *         max_count == 0 or count > max_count
 */
static mini_os_err_t mini_os_semaphore_init(mini_os_semaphore_t* semaphore,
                                            mini_os_uint16_t max_count,
                                            mini_os_uint16_t count,
                                            mini_os_bool_t is_static,
                                            const char* name)
{
    if (semaphore == MINI_OS_NULL || max_count == 0u || count > max_count)
    {
        return MINI_OS_ERR_INVAL;
    }

    mini_os_semaphore_name_set(semaphore->name, name);
    semaphore->max_count = max_count;
    semaphore->count = count;
    semaphore->is_static = is_static;
    mini_os_list_init(&semaphore->wait_list);

#if MINI_OS_FIND_BY_NAME
    {
        mini_os_irq_t irq = mini_os_irq_save();

        mini_os_list_init(&semaphore->g_list_node);
        mini_os_list_tail(&semaphore->g_list_node, &g_semaphore_list);
        mini_os_irq_restore(irq);
    }
#endif
    return MINI_OS_OK;
}

mini_os_semaphore_t* mini_os_semaphore_create(const char* name, mini_os_uint16_t max_count, mini_os_uint16_t count)
{
    mini_os_semaphore_t* semaphore = (mini_os_semaphore_t*)mini_os_malloc(sizeof(mini_os_semaphore_t));

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    if (mini_os_semaphore_init(semaphore, max_count, count, MINI_OS_FALSE, name) != MINI_OS_OK)
    {
        (void)mini_os_free(semaphore);  /* init failed: give the block back, no leak */
        return MINI_OS_NULL;
    }
    return semaphore;
}

mini_os_semaphore_t* mini_os_binary_semaphore_create(const char* name)
{
    mini_os_semaphore_t* semaphore = (mini_os_semaphore_t*)mini_os_malloc(sizeof(mini_os_semaphore_t));

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    if (mini_os_semaphore_init(semaphore, 1u, 1u, MINI_OS_FALSE, name) != MINI_OS_OK)
    {
        (void)mini_os_free(semaphore);
        return MINI_OS_NULL;
    }
    return semaphore;
}

mini_os_semaphore_t* mini_os_semaphore_create_static(const char* name,
                                                     mini_os_uint16_t max_count,
                                                     mini_os_uint16_t count,
                                                     mini_os_semaphore_t* semaphore)
{
    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    if (mini_os_semaphore_init(semaphore, max_count, count, MINI_OS_TRUE, name) != MINI_OS_OK)
    {
        return MINI_OS_NULL;
    }
    return semaphore;
}

mini_os_semaphore_t* mini_os_binary_semaphore_create_static(const char* name, mini_os_semaphore_t* semaphore)
{
    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    if (mini_os_semaphore_init(semaphore, 1u, 1u, MINI_OS_TRUE, name) != MINI_OS_OK)
    {
        return MINI_OS_NULL;
    }
    return semaphore;
}

mini_os_err_t mini_os_semaphore_to_binary(mini_os_semaphore_t* semaphore)
{
    mini_os_err_t ret = MINI_OS_OK;
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    /* Only a saturated semaphore can be collapsed: count == max_count means
     * every unit is home and nobody still owes a give, so shrinking the
     * capacity to 1 cannot swallow an outstanding unit. A parked waiter is
     * rejected too (it implies count == 0,  all units are held). */
    if (semaphore->count != semaphore->max_count || !mini_os_list_is_empty(&semaphore->wait_list))
    {
        ret = MINI_OS_ERR_BUSY;
    }
    else
    {
        semaphore->max_count = 1u;
        semaphore->count = 1u;
    }
    mini_os_irq_restore(irq);
    return ret;
}

mini_os_err_t mini_os_semaphore_to_counting(mini_os_semaphore_t* semaphore, mini_os_uint16_t max_count)
{
    mini_os_err_t ret = MINI_OS_OK;
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL || max_count < 2u)
    {
        return MINI_OS_ERR_INVAL;  
    }

    irq = mini_os_irq_save();
    if (semaphore->max_count != 1u)
    {
        ret = MINI_OS_ERR_NOTSUPP;  
    }
    else
    {
        /* Widening can never swallow a unit, so the count is left as it is: an
         * outstanding one (count 0) stays owed by its holder, a published one
         * (count 1) stays available to the next taker. */
        semaphore->max_count = max_count;
    }
    mini_os_irq_restore(irq);
    return ret;
}

mini_os_err_t mini_os_semaphore_delete(mini_os_semaphore_t* semaphore)
{
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (semaphore->is_static != MINI_OS_FALSE)
    {
        return MINI_OS_ERR_NOTSUPP;     /* caller-provided storage: use delete_static */
    }

    irq = mini_os_irq_save();
    if (!mini_os_list_is_empty(&semaphore->wait_list))
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;        /* threads still parked: waking them is the caller's job */
    }
#if MINI_OS_FIND_BY_NAME
    mini_os_list_remove(&semaphore->g_list_node);
#endif
    mini_os_irq_restore(irq);

    (void)mini_os_free(semaphore);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_semaphore_delete_static(mini_os_semaphore_t* semaphore)
{
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    if (semaphore->is_static == MINI_OS_FALSE)
    {
        return MINI_OS_ERR_NOTSUPP;     /* heap owned: use mini_os_semaphore_delete */
    }

    irq = mini_os_irq_save();
    if (!mini_os_list_is_empty(&semaphore->wait_list))
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;
    }
#if MINI_OS_FIND_BY_NAME
    mini_os_list_remove(&semaphore->g_list_node);
#endif
    /* never mini_os_free() here: the storage belongs to the caller, only clear it */
    MINI_OS_MEMSET(semaphore, 0, sizeof(mini_os_semaphore_t));
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_semaphore_get_count(mini_os_semaphore_t* semaphore, mini_os_uint16_t* count)
{
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL || count == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    *count = semaphore->count;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_semaphore_take(mini_os_semaphore_t* semaphore, mini_os_tick_t timeout_tick)
{
    mini_os_err_t parked;
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    if (semaphore->count > 0u)
    {
        semaphore->count--;             
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
    /* Park inside the critical section so a give cannot slip between the count
     * check and the park; park consumes the critical section and yields.
     * No retry loop and no deadline recomputation: MINI_OS_OK means the giver
     * handed the unit to this thread directly, so the count is not touched
     * again here  */
    parked = mini_os_sync_wait_park(&semaphore->wait_list, 0u, timeout_tick, irq);
    if (parked == MINI_OS_OK)
    {
        return MINI_OS_OK;
    }
    return (parked == MINI_OS_ERR_TIMEOUT) ? MINI_OS_ERR_TIMEOUT : parked;
}

mini_os_err_t mini_os_semaphore_try_take(mini_os_semaphore_t* semaphore)
{
    return mini_os_semaphore_take(semaphore, 0);   
}

mini_os_err_t mini_os_semaphore_give(mini_os_semaphore_t* semaphore)
{
    mini_os_bool_t woken;
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    woken = mini_os_semaphore_wake_one(semaphore);
    if (woken != MINI_OS_FALSE)
    {
        mini_os_irq_restore(irq);
        (void)mini_os_schedule_yield(); /* the woken thread may outrank the caller */
        return MINI_OS_OK;
    }
    /* No waiter: publish the unit for the next taker, up to the capacity. */
    if (semaphore->count >= semaphore->max_count)
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;    /* saturated: the semaphore already holds max_count units */
    }
    semaphore->count++;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_semaphore_give_isr(mini_os_semaphore_t* semaphore)
{
    mini_os_bool_t woken;
    mini_os_irq_t irq;

    if (semaphore == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    woken = mini_os_semaphore_wake_one(semaphore);
    if (woken != MINI_OS_FALSE)
    {
        mini_os_irq_restore(irq);
        return MINI_OS_OK;          /* unit handed straight to the oldest waiter */
    }
    if (semaphore->count >= semaphore->max_count)
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;
    }
    semaphore->count++;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

#if MINI_OS_FIND_BY_NAME
mini_os_semaphore_t* mini_os_get_semaphore_by_name(const char* name)
{
    mini_os_list_t* node;

    if (name == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    for (node = g_semaphore_list.next; node != &g_semaphore_list; node = node->next)
    {
        mini_os_semaphore_t* semaphore = mini_os_container_of(node, mini_os_semaphore_t, g_list_node);

        if (MINI_OS_STRCMP(semaphore->name, name) == 0)
        {
            return semaphore;
        }
    }
    return MINI_OS_NULL;
}
#endif
