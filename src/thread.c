/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @author H-000-H
 * @file thread.c
 * @brief Thread management functions
 */
#include "thread.h"
#include "err.h"
#include "list.h"
#include "mini_config.h"
#include "port.h"
#include "redef.h"
#include "memory.h"
#include "schedule.h"
#include <stdint.h>
#define IDLE_THREAD_NAME "idle_thread"
#if MINI_OS_FIND_BY_NAME
static mini_os_list_t g_threads_list;
MINI_OS_CONSTRUCTOR(MINI_OS_FIND_BY_NAME_CONSTRUCTOR) void mini_os_global_list_init(void)
{
    mini_os_list_init(&g_threads_list);
}
#endif
static void mini_os_thread_entry_wrapper(void *param);
static void *mini_os_thread_stack_init(
    mini_os_uint8_t *stack_buf,
    mini_os_uint32_t stack_size,
    void *param
)
{
    volatile mini_os_uint32_t *sp = (volatile mini_os_uint32_t *)(stack_buf + stack_size);
    *(--sp) = 0x01000000U;                  /**< xPSR */
    *(--sp) = (mini_os_uint32_t)mini_os_thread_entry_wrapper | 1u; /**< PC: wrapper (runs entry, then cleanup + exit) */
    *(--sp) = 0xFFFFFFFFU;                  /**< LR */
    *(--sp) = 0U;                           /**< R12 */
    *(--sp) = 0U;                           /**< R3 */
    *(--sp) = 0U;                           /**< R2 */
    *(--sp) = 0U;                           /**< R1 */
    *(--sp) = (mini_os_uint32_t)param;      /**< R0: param entry */

    *(--sp) = 0U;                           /**< r11 */
    *(--sp) = 0U;                           /**< r10 */
    *(--sp) = 0U;                           /**< r9 */
    *(--sp) = 0U;                           /**< r8 */
    *(--sp) = 0U;                           /**< r7 */
    *(--sp) = 0U;                           /**< r6 */
    *(--sp) = 0U;                           /**< r5 */
    *(--sp) = 0U;                           /**< r4 */
    return (void*)sp;
}

static mini_os_err_t mini_os_thread_init(
    mini_os_thread_t *thread,
    const char* name,
    mini_os_size_t stack_size,
    mini_os_uint8_t priority,
    void (*entry)(void *),
    void* param,
    mini_os_uint32_t* stack_buffer
)
{

    if (entry == MINI_OS_NULL
        || priority >= MINI_OS_PRIORITY
        || stack_size < MINI_OS_THREAD_MIN_STACK_SIZE
        || (stack_size & 7U) != 0U
        || stack_buffer == MINI_OS_NULL
        || ((mini_os_size_t)stack_buffer & 7U) != 0U
        || thread == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    /* thread name (bounded copy, always NUL-terminated) */
    if (name == MINI_OS_NULL)
    {
        thread->thread_name[0] = '\0';
    }
    else
    {
        mini_os_size_t i = 0u;
        while (i < (mini_os_size_t)(THREADS_NAME_LEN - 1) && name[i] != '\0')
        {
            thread->thread_name[i] = name[i];
            i++;
        }
        thread->thread_name[i] = '\0';
    }

    thread->entry = entry;
    thread->param = param;
    thread->stack_size = stack_size;
    thread->stack_addr = stack_buffer;
    thread->priority = priority;
#if MINI_OS_FIND_BY_NAME
    mini_os_list_init(&thread->g_list_node);
    mini_os_list_tail(&thread->g_list_node, &g_threads_list);
#endif
    thread->state = MINI_OS_THREAD_STATE_INIT;
    thread->err = MINI_OS_OK;

    thread->sp = mini_os_thread_stack_init((mini_os_uint8_t*)stack_buffer, stack_size, param);
    thread->round = 0;
    thread->resume_time = 0;
    thread->wheel_slot = (mini_os_uint8_t)MINI_OS_TICK_WHEEL;
#if MINI_OS_TIME_SLICE
    thread->remain_tick = 0;
    thread->init_tick_num = 0;
#endif

    /* list nodes must be self-referencing (mini-os list convention) */
    mini_os_list_init(&thread->list_node);

    thread->thread_cleanup = MINI_OS_NULL;
    thread->user_data = 0;

#if MINI_OS_THREAD_DETACH
    /* detach by default: idle reaps every corpse unless a joiner is involved */
    thread->is_detach = MINI_OS_TRUE;
    thread->is_terminated = MINI_OS_FALSE;
    thread->exit_retval = MINI_OS_NULL;
    thread->join_wait_sem = MINI_OS_NULL;
#endif

    return MINI_OS_OK;
}

/* Current thread TCB pointer. Shared with the port assembly (port.c);
 * the public getter mini_os_thread_current() below returns it. */
mini_os_thread_t *mini_os_current_thread = MINI_OS_NULL;

mini_os_thread_t *mini_os_thread_current(void)
{
    return mini_os_current_thread;
}

/* Corpses of terminated threads, drained (freed) by the idle thread */
static mini_os_list_t s_defunct_list;

/**
 * @brief Enqueue a terminated thread into the corpse queue
 * @param[in] thread thread that has just been marked TERMINATED
 * @note caller must hold the IRQ lock; lazy-inits the queue on first use;
 *       the corpse stays linked here until the idle thread reclaims it
 */
static void mini_os_defunct_list_insert(mini_os_thread_t *thread)
{
    if (s_defunct_list.next == MINI_OS_NULL)
    {
        mini_os_list_init(&s_defunct_list); /* lazy init (constructor-free) */
    }
    mini_os_list_tail(&thread->list_node, &s_defunct_list);
}

/**
 * @brief Run the user entry, then cleanup and exit
 * @param[in] param argument passed to the thread entry function
 * @note pushed as the initial PC so a returning entry is handled, not faulted
 */
static void mini_os_thread_entry_wrapper(void *param)
{
    mini_os_thread_t *thread = mini_os_current_thread;

    thread->entry(param);

    if (thread->thread_cleanup != MINI_OS_NULL)
    {
        thread->thread_cleanup(param);
    }

    mini_os_thread_exit(MINI_OS_NULL);
}

/**
 * @brief Reclaim terminated threads (called from the idle thread)
 * @note heap-backed TCB/stack are freed; caller-owned (static) memory is left
 *       untouched because mini_os_free() rejects non-heap pointers
 */
static void mini_os_thread_defunct_execute(void)
{
    mini_os_irq_t irq = mini_os_irq_save();

    while (!mini_os_list_is_empty(&s_defunct_list))
    {
        mini_os_list_t *node = s_defunct_list.next;
        mini_os_thread_t *dead;

        mini_os_list_remove(node);
        dead = mini_os_container_of(node, mini_os_thread_t, list_node);
#if MINI_OS_FIND_BY_NAME
        mini_os_list_remove(&dead->g_list_node);
#endif
        mini_os_free(dead->stack_addr);
        mini_os_free(dead);
    }
    mini_os_irq_restore(irq);
}

MINI_OS_NO_RETURN void mini_os_thread_exit(void *retval)
{
    mini_os_thread_t *thread = mini_os_current_thread;
    mini_os_irq_t irq;

    if (thread == MINI_OS_NULL)
    {
        while (1) { } /* fatal: exit called outside a thread */
    }

    irq = mini_os_irq_save();

    /* a running thread is still linked in the ready/running list */
    mini_os_remove_thread_from_ready_running_list(thread);

    thread->state = MINI_OS_THREAD_STATE_TERMINATED;
#if MINI_OS_THREAD_DETACH
    thread->is_terminated = MINI_OS_TRUE;
    thread->exit_retval = retval;
    /* TODO: wake join_wait_sem waiters once the semaphore is implemented */
#else
    (void)retval; /* join fields are compiled out without MINI_OS_THREAD_DETACH */
#endif

    /* corpse queue: reclaimed by the idle thread */
    mini_os_defunct_list_insert(thread);

    mini_os_irq_restore(irq);
    mini_os_schedule_yield(); /* switch away, never return */
    while (1) { }
}

/**
 * @brief Suspend a thread
 * @param[in] thread Thread to suspend
 * @return mini_os_err_t on success, 0 on failure
 * @note
 *  - a READY/RUNNING thread is unlinked from the ready/running list
 *  - a BLOCKED thread is unlinked from its wait list; a wheel-parked one
 *    keeps the remaining delay in resume_time (frozen), a sync-object wait
 *    is canceled
 *  - suspending the current thread triggers a context switch
 */
mini_os_err_t mini_os_thread_suspend(                               mini_os_thread_t* thread)
{
    if (thread == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    if (thread->state == MINI_OS_THREAD_STATE_SUSPENDED)
    {
        return MINI_OS_OK;
    }

    mini_os_irq_t irq = mini_os_irq_save();
    if (thread->state == MINI_OS_THREAD_STATE_READY ||
        thread->state == MINI_OS_THREAD_STATE_RUNNING)
    {
        mini_os_remove_thread_from_ready_running_list(thread);
    }
    else if (thread->state == MINI_OS_THREAD_STATE_BLOCKED)
    {
        /* Capture the remaining delay before unlinking: a wheel-parked thread
         * keeps its countdown frozen in resume_time; a sync-object waiter
         * (wheel_slot out of range) is canceled (resume_time = 0). */
        thread->resume_time = (mini_os_tick_t)mini_os_wheel_remain(thread);
        if (!mini_os_list_is_empty(&thread->list_node))
        {
            mini_os_list_remove(&thread->list_node);
        }
        thread->wheel_slot = (mini_os_uint8_t)MINI_OS_TICK_WHEEL;
        thread->round = 0;
    }
    else /**< INIT/TERMINATED: nothing to detach, nothing to suspend */
    {
        mini_os_irq_restore(irq);
        return MINI_OS_ERR_BUSY;
    }

    thread->state = MINI_OS_THREAD_STATE_SUSPENDED;

    mini_os_irq_restore(irq);
    if (thread == mini_os_current_thread)
    {
        mini_os_schedule_yield();
    }
    return MINI_OS_OK;
}

/**
 * @brief Resume a suspended thread
 * @param[in] thread Thread to resume
 * @return mini_os_err_t on success, MINI_OS_ERR_INVAL on invalid arguments
 * @note
 *  - with a captured resume_time: re-parked in the time wheel (BLOCKED),
 *    the frozen delay continues exactly
 *  - otherwise: put back into the ready/running list
 */
mini_os_err_t mini_os_thread_resume(                                mini_os_thread_t* thread)
{
    if (thread == MINI_OS_NULL || thread->state != MINI_OS_THREAD_STATE_SUSPENDED)
    {
        return MINI_OS_ERR_INVAL;
    }

    mini_os_irq_t irq = mini_os_irq_save();
    if (thread->resume_time > 0)
    {
        /* was wheel-suspended: re-park with the exact remaining ticks */
        mini_os_wheel_insert(thread, (mini_os_uint32_t)thread->resume_time);
        thread->resume_time = 0;
    }
    else
    {
        /* suspended from the ready list or a canceled sync wait: back to ready */
        mini_os_add_thread_to_ready_running_list(thread);
    }
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

mini_os_thread_t* mini_os_thread_create(                            const char* name,
                                                                    mini_os_uint32_t stack_size,
                                                                    mini_os_uint8_t priority,
                                                                    void (*entry)(void *),
                                                                    void* param)
{
    mini_os_thread_t* thread;
    mini_os_uint32_t* stack;

    if (!name || stack_size == 0 || priority >= MINI_OS_PRIORITY || !entry)
    {
        return MINI_OS_NULL;
    }

    thread = (mini_os_thread_t*)mini_os_malloc(sizeof(mini_os_thread_t));
    if (thread == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }

    stack = (mini_os_uint32_t*)mini_os_malloc(stack_size);
    if (stack == MINI_OS_NULL)
    {
        mini_os_free(thread);
        return MINI_OS_NULL;
    }

    if (mini_os_thread_init(thread, name, stack_size, priority, entry, param, stack) != MINI_OS_OK)
    {
        mini_os_free(stack);
        mini_os_free(thread);
        return MINI_OS_NULL;
    }
    /* auto-start: the thread becomes ready immediately */
    if (mini_os_add_thread_to_ready_running_list(thread) != MINI_OS_OK)
    {
        mini_os_free(stack);
        mini_os_free(thread);
        return MINI_OS_NULL;
    }
    return thread;
}

mini_os_err_t mini_os_thread_delete(mini_os_thread_t *thread)
{
    mini_os_irq_t irq;

    if (thread == MINI_OS_NULL || thread == mini_os_current_thread ||
        thread->state == MINI_OS_THREAD_STATE_TERMINATED)
    {
        return MINI_OS_ERR_INVAL; /* running thread; TERMINATED ones are reclaimed by idle */
    }

    irq = mini_os_irq_save();
    if (thread->state == MINI_OS_THREAD_STATE_READY ||
        thread->state == MINI_OS_THREAD_STATE_RUNNING)
    {
        mini_os_remove_thread_from_ready_running_list(thread);
    }
    else if (thread->state == MINI_OS_THREAD_STATE_BLOCKED)
    {
        mini_os_remove_thread_from_blocked_list(thread);
    }
#if MINI_OS_FIND_BY_NAME
    mini_os_list_remove(&thread->g_list_node);
#endif
    /* TODO: remove from semaphore wait queues */
    mini_os_irq_restore(irq);

    mini_os_free(thread->stack_addr);
    mini_os_free(thread);
    return MINI_OS_OK;
}

mini_os_thread_t* mini_os_thread_create_static(                     const char* name,
                                                                    mini_os_uint32_t stack_size,
                                                                    mini_os_uint8_t priority,
                                                                    void (*entry)(void *),
                                                                    void* param,
                                                                    mini_os_uint32_t* stack_buffer,
                                                                    mini_os_thread_t* task_buffer)
{
    mini_os_size_t aligned;

    if (!name || stack_size == 0 || priority >= MINI_OS_PRIORITY || !entry ||
        !stack_buffer || !task_buffer)
    {
        return MINI_OS_NULL;
    }
    /* the stack must go through mini_os_stack_create first (8-byte alignment gate) */
    if (mini_os_stack_create(stack_size, stack_buffer, &aligned) == MINI_OS_NULL)
    {
        return MINI_OS_NULL;
    }
    if (mini_os_thread_init(task_buffer, name, stack_size, priority, entry, param, stack_buffer) != MINI_OS_OK)
    {
        return MINI_OS_NULL;
    }
    /* auto-start: the thread becomes ready immediately */
    if (mini_os_add_thread_to_ready_running_list(task_buffer) != MINI_OS_OK)
    {
        return MINI_OS_NULL;
    }
    return task_buffer;
}

mini_os_err_t mini_os_thread_delete_static(                         mini_os_thread_t* thread,
                                                                    mini_os_uint32_t* stack_buffer,
                                                                    mini_os_thread_t* task_buffer)
{
    mini_os_size_t stack_size;
    mini_os_uint32_t *stack_addr;

    if (!thread || !stack_buffer || !task_buffer || thread == mini_os_current_thread ||
        thread->state == MINI_OS_THREAD_STATE_TERMINATED)
    {
        return MINI_OS_ERR_INVAL; /* running thread; TERMINATED ones are reclaimed by idle */
    }
    /* thread == task_buffer: capture fields before the TCB is zeroed */
    stack_size = thread->stack_size;
    stack_addr = thread->stack_addr;

    mini_os_irq_t irq = mini_os_irq_save();
    if (thread->state == MINI_OS_THREAD_STATE_READY ||
        thread->state == MINI_OS_THREAD_STATE_RUNNING)
    {
        mini_os_remove_thread_from_ready_running_list(thread);
    }
    else if (thread->state == MINI_OS_THREAD_STATE_BLOCKED)
    {
        mini_os_remove_thread_from_blocked_list(thread);
    }
#if MINI_OS_FIND_BY_NAME
    mini_os_list_remove(&thread->g_list_node);
#endif

    MINI_OS_MEMSET(task_buffer, 0, sizeof(mini_os_thread_t));
    MINI_OS_MEMSET(stack_addr, 0, stack_size);
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

#if MINI_OS_THREAD_DETACH
/**
 * @brief Detach a thread
 * @param[in] thread Thread to detach
 * @return MINI_OS_OK on success; MINI_OS_ERR_INVAL when thread is MINI_OS_NULL
 * @note default semantics are already detach-like: idle reaps every corpse, so
 *       detaching only clears the joinable expectation for future join support
 */
mini_os_err_t mini_os_thread_detach(mini_os_thread_t* thread)
{
    mini_os_irq_t irq;

    if (thread == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }

    irq = mini_os_irq_save();
    thread->is_detach = MINI_OS_TRUE;
    mini_os_irq_restore(irq);
    return MINI_OS_OK;
}

/**
 * @brief Join a thread
 * @param[in] thread Thread to join
 * @param[out] thread_return Buffer to store the thread return value in
 * @param[in] timeout_tick Timeout tick for the join operation
 * @return MINI_OS_OK when the thread has terminated and the value was collected;
 *         MINI_OS_ERR_INVAL on invalid args or self-join;
 *         MINI_OS_ERR_NOTSUPP while the thread is still running
 * @note TODO: blocking join needs a semaphore on join_wait_sem; until then only
 *       already-terminated threads can be joined (non-blocking poll). The
 *       corpse is still reclaimed by idle, so the caller must join before idle
 *       frees the TCB.
 */
mini_os_err_t mini_os_thread_join(                          mini_os_thread_t* thread,
                                                            void **thread_return,
                                                            mini_os_tick_t timeout_tick)
{
    mini_os_bool_t terminated;
    void *retval;
    mini_os_irq_t irq;

    if (thread == MINI_OS_NULL || thread == mini_os_current_thread)
    {
        return MINI_OS_ERR_INVAL; /* invalid arg or self-join deadlock */
    }

    irq = mini_os_irq_save();
    terminated = thread->is_terminated;
    retval = thread->exit_retval;
    mini_os_irq_restore(irq);

    if (!terminated)
    {
        /* TODO: wait on thread->join_wait_sem once the semaphore module is
         * ready; the wake-up side is already stubbed in mini_os_thread_exit() */
        (void)timeout_tick;
        return MINI_OS_ERR_NOTSUPP;
    }

    if (thread_return != MINI_OS_NULL)
    {
        *thread_return = retval;
    }
    return MINI_OS_OK;
}
#endif /* MINI_OS_THREAD_DETACH */

mini_os_err_t mini_os_thread_yield(                                 void)
{
    mini_os_yield_trigger();
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_delay_tick(                            mini_os_uint32_t ticks)
{
    if (ticks == 0)
        return MINI_OS_OK;
    mini_os_schedule_delay(ticks);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_delay_ms(                              mini_os_uint32_t ms)
{
    if (ms == 0)
        return MINI_OS_OK;
    mini_os_schedule_delay(MINI_OS_MS_TO_TICK(ms));
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_delay_tick_until(                      mini_os_uint32_t ticks)
{
    if (ticks == 0)
        return MINI_OS_OK;

    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_set_name(                              mini_os_thread_t* thread, const char* name)
{
    if (!thread )
        return MINI_OS_ERR_INVAL;
    if(name==MINI_OS_NULL)
        thread->thread_name[0] = '\0';
    else
    {
        uint8_t len = 0;
        for (uint8_t i = 0; i < (mini_os_size_t)(THREADS_NAME_LEN - 1) && name[i] != '\0';i++)
        {
            thread->thread_name[i] = name[i];
            len++;
        }
        thread->thread_name[len] = '\0';
    }
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_get_name(                              mini_os_thread_t* thread, char* name, mini_os_uint32_t* name_len)
{
    if (!thread || !name || !name_len)
        return MINI_OS_ERR_INVAL;
    uint8_t len = 0;
    for (uint8_t i = 0; i < (mini_os_size_t)(THREADS_NAME_LEN - 1) && thread->thread_name[i] != '\0'; i++)
    {
        name[i] = thread->thread_name[i];
        len++;
    }
    name[len] = '\0';
    *name_len = len;
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_set_priority(                          mini_os_thread_t* thread, mini_os_uint8_t priority)
{
    if (!thread || priority >= MINI_OS_PRIORITY)
        return MINI_OS_ERR_INVAL;
    thread->priority = priority;
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_get_priority(                          mini_os_thread_t* thread, mini_os_uint8_t* priority)
{
    if (!thread || !priority)
        return MINI_OS_ERR_INVAL;
    *priority = thread->priority;
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_get_state(mini_os_thread_t* thread, mini_os_thread_state_t *state)
{
    if (!thread || !state)
        return MINI_OS_ERR_INVAL;
    *state = thread->state;
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_set_user_data(mini_os_thread_t* thread, mini_os_user_data_t user_data)
{
    if (!thread)
        return MINI_OS_ERR_INVAL;
    thread->user_data = user_data;
    return MINI_OS_OK;
}

mini_os_err_t mini_os_thread_set_cleanup(mini_os_thread_t* thread, void (*cleanup)(void *), void *arg)
{
    (void)arg; /* cleanup is invoked with the thread entry param by the wrapper */
    if (!thread)
        return MINI_OS_ERR_INVAL;
    thread->thread_cleanup = cleanup;

    return MINI_OS_OK;
}

#if MINI_OS_FIND_BY_NAME
mini_os_thread_t* mini_os_find_by_name(                             const char* name)
{
    if (!name)
    {
        return MINI_OS_NULL;
    }
    for (mini_os_list_t* node = g_threads_list.next; node != &g_threads_list; node = node->next)
    {
        mini_os_thread_t* thread = mini_os_container_of(node, mini_os_thread_t, g_list_node);
        if (MINI_OS_STRCMP(thread->thread_name, name) == 0)
        {
            return thread;
        }
    }
    return MINI_OS_NULL;
}
#endif
#if MINI_OS_TIME_SLICE
/**
 * @brief Set the time slice of a thread (ticks per round-robin quantum)
 * @param[in] thread thread to configure
 * @param[in] tick slice length in ticks; 0 = no slice limit
 * @return MINI_OS_OK on success; MINI_OS_ERR_INVAL when thread is MINI_OS_NULL
 * @note only has an effect when MINI_OS_TIME_SLICE is enabled
 */
mini_os_err_t mini_os_thread_set_timeslice(mini_os_thread_t* thread, mini_os_tick_t tick)
{
    if (thread == MINI_OS_NULL || tick < 0)
    {
        return MINI_OS_ERR_INVAL;
    }
    thread->init_tick_num = tick;
    thread->remain_tick = tick;
    return MINI_OS_OK;
}

/**
 * @brief Get the configured time slice of a thread
 * @param[in] thread thread to query
 * @param[out] tick receives the configured slice length in ticks
 * @return MINI_OS_OK on success; MINI_OS_ERR_INVAL when thread/tick is MINI_OS_NULL
 */
mini_os_err_t mini_os_thread_get_timeslice(mini_os_thread_t* thread, mini_os_tick_t* tick)
{
    if (thread == MINI_OS_NULL || tick == MINI_OS_NULL)
    {
        return MINI_OS_ERR_INVAL;
    }
    *tick = thread->init_tick_num;
    return MINI_OS_OK;
}
#endif

MINI_OS_WEAK mini_os_err_t mini_os_thread_idle_hook(idle_hook_t hook, void* param)
{
    while (1)
    {
        mini_os_thread_defunct_execute(); /* reclaim terminated threads */
        if (hook != MINI_OS_NULL)
        {
            hook(param);
        }
        __asm__ volatile ("wfi":::"memory");
    }
    return MINI_OS_OK;
}

MINI_OS_WEAK void mini_os_thread_idle(void*param)
{
    mini_os_thread_idle_hook(MINI_OS_NULL, param);
}

MINI_OS_WEAK void mini_os_thread_idle_create(void)
{
    mini_os_thread_create(IDLE_THREAD_NAME, MINI_OS_DEFAULT_IDLE_STACK_SIZE, MINI_OS_PRIORITY_NUM-1, mini_os_thread_idle, MINI_OS_NULL);
}
