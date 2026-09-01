/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file schedule.c
 * @brief Scheduler implementation
 * @author H-000-H
 * @details
 *  set thread A B C(same priority)
 *  -1：current is B (middle)
 *   sentinal → A → B(current) → C → sentinal
 *   current->next = C  → not sentinal → choose C ✓
 *
 *  -2：current is C (tail)
 *   sentinal → A → B → C(current) → sentinal
 *   current->next = sentinal → is sentinal → get sentinal.next = A → choose A（loop around）✓
 *
 *  -3：only one task
 *   sentinal → A(current thread) → sentinal
 *   current->next = sentinal → get sentinal.next = A → also self ✓
 *
 */
#include "schedule.h"
#include "err.h"
#include "list.h"
#include "mini_config.h"
#include "port.h"
#include "redef.h"
#include "thread.h"

/* Global ready bitmap (declared extern in schedule.h / thread.h) */
mini_os_uint32_t g_priority = 0u;

/* Ready/running list head per priority (declared extern in schedule.h) */
mini_os_list_t g_ready_running_list[MINI_OS_PRIORITY];

extern mini_os_thread_t *mini_os_current_thread;

static mini_os_uint8_t s_current_priority = 0;

mini_os_uint32_t g_global_tick = 0;
#ifdef MINI_OS_LONG_TIME
mini_os_uint32_t g_global_tick_overflow = 0;
#endif
/* Time wheel (hierarchical tick wheel). MINI_OS_TICK_WHEEL must be a power of 2. */
MINI_OS_ASSERT((MINI_OS_TICK_WHEEL & MINI_OS_TICK_WHEEL_MASK) == 0, "MINI_OS_TICK_WHEEL must be a power of 2");

static mini_os_list_t s_wheel[MINI_OS_TICK_WHEEL];
static mini_os_uint32_t s_current_slot = 0;

mini_os_err_t mini_os_schedule_init(void)
{
    mini_os_uint32_t i;

    g_priority = 0;
    for (i = 0; i < (mini_os_uint32_t)MINI_OS_PRIORITY; i++)
    {
        mini_os_list_init(&g_ready_running_list[i]);
    }
    for (i = 0; i < (mini_os_uint32_t)MINI_OS_TICK_WHEEL; i++)
    {
        mini_os_list_init(&s_wheel[i]);
    }
    s_current_slot = 0;
    return MINI_OS_OK;
}

mini_os_err_t mini_os_schedule_start(void)
{
    MINI_OS_PENDSV_IRQ = 0xFF;   /* PendSV: lowest priority (never preempts user IRQs) */
    MINI_OS_SYSTICK_IRQ = 0xFE;  /* SysTick: second-lowest */
    mini_os_psp_set(MINI_OS_NONE_THREAD_TO_RESTORE);
    mini_os_set_control(MINI_OS_CONTROL_REGISTER_PSP_PRIVILEGE);
    mini_os_yield_trigger();
    mini_os_irq_enable();
    return MINI_OS_OK;
}

mini_os_err_t mini_os_schedule_switch(void)
{
    /* Switch to the next ready thread, if any */
    mini_os_uint8_t old_priority;
    mini_os_uint8_t next_priority;
    mini_os_list_t *next_node;
    mini_os_thread_t *next_thread;

    old_priority = s_current_priority;
    if (mini_os_current_thread != MINI_OS_NULL)
    {
        if (mini_os_current_thread->state == MINI_OS_THREAD_STATE_RUNNING)
        {
            mini_os_current_thread->state = MINI_OS_THREAD_STATE_READY;
        }
    }
    next_priority = mini_os_get_highest_priority();
    if (next_priority >= (mini_os_uint8_t)MINI_OS_PRIORITY)
    {
        return MINI_OS_ERR_NODEV;
    }
    s_current_priority = next_priority;

    if (mini_os_current_thread != MINI_OS_NULL &&
        next_priority == old_priority &&
        mini_os_current_thread->state == MINI_OS_THREAD_STATE_READY &&
        mini_os_current_thread->list_node.next != &mini_os_current_thread->list_node)
    {
        /* Same priority, current thread still runnable and linked: round-robin
         * by advancing to its successor (wrap past the sentinel at the tail). */
        next_node = mini_os_current_thread->list_node.next;
        if (next_node == &g_ready_running_list[s_current_priority])
        {
            next_node = next_node->next;
        }
    }
    else
    {
        /* Different priority (preemption), or the current thread is no longer
         * runnable (suspended/blocked/terminated) or left the list: take the
         * head of the selected priority list directly. */
        next_node = g_ready_running_list[s_current_priority].next;
    }

    next_thread = mini_os_container_of(next_node, mini_os_thread_t, list_node);
    next_thread->state = MINI_OS_THREAD_STATE_RUNNING;
    mini_os_current_thread = next_thread; /* the port restores SP from this */
    return MINI_OS_OK;
}

mini_os_err_t mini_os_schedule_yield(void)
{
    mini_os_irq_t irq_level = mini_os_irq_save();
    mini_os_yield_trigger();
    mini_os_irq_restore(irq_level);
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    return MINI_OS_OK;
}

mini_os_err_t mini_os_add_thread_to_ready_running_list(mini_os_thread_t *thread)
{
    if (!thread || thread->state == MINI_OS_THREAD_STATE_READY ||
        thread->state == MINI_OS_THREAD_STATE_RUNNING || thread->priority >= MINI_OS_PRIORITY)
    {
        return MINI_OS_ERR_INVAL;
    }
    mini_os_irq_t irq_level = mini_os_irq_save();
    thread->state = MINI_OS_THREAD_STATE_READY;
    mini_os_list_tail(&thread->list_node, &g_ready_running_list[thread->priority]);
    g_priority |= (1u << thread->priority);
    mini_os_irq_restore(irq_level);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_remove_thread_from_ready_running_list(mini_os_thread_t *thread)
{
    if (!thread || (thread->state != MINI_OS_THREAD_STATE_READY &&
        thread->state != MINI_OS_THREAD_STATE_RUNNING) ||
        thread->priority >= MINI_OS_PRIORITY)
    {
        return MINI_OS_ERR_INVAL;
    }

    mini_os_irq_t irq_level = mini_os_irq_save();

    mini_os_list_remove(&thread->list_node);

    if (mini_os_list_is_empty(&g_ready_running_list[thread->priority]))
    {
        g_priority &= ~(1u << thread->priority);
    }

    mini_os_irq_restore(irq_level);
    return MINI_OS_OK;
}

mini_os_err_t mini_os_remove_thread_from_blocked_list(mini_os_thread_t *thread)
{
    if (!thread || thread->state != MINI_OS_THREAD_STATE_BLOCKED)
    {
        return MINI_OS_ERR_INVAL;
    }

    mini_os_irq_t irq_level = mini_os_irq_save();

    mini_os_list_remove(&thread->list_node);
    thread->round = 0;
    thread->wheel_slot = (mini_os_uint8_t)MINI_OS_TICK_WHEEL;

    mini_os_irq_restore(irq_level);
    return MINI_OS_OK;
}

/**
 * @ time wheel tick scheduler
 */
static void mini_os_tick_decrement(void)
{
    mini_os_list_t *node, *next;
    mini_os_thread_t *thread;
    s_current_slot = (s_current_slot + 1) & MINI_OS_TICK_WHEEL_MASK;/* increment current slot */

    for(node = s_wheel[s_current_slot].next; node != &s_wheel[s_current_slot]; node = next)
    {
        next = node->next;
        thread = mini_os_container_of(node,mini_os_thread_t,list_node);
        if(thread->round > 0)
        {
            thread->round--;
            continue;
        }

        mini_os_list_remove(&thread->list_node);
        thread->wheel_slot = (mini_os_uint8_t)MINI_OS_TICK_WHEEL;
        mini_os_add_thread_to_ready_running_list(thread);
    }
}

mini_os_err_t mini_os_wheel_insert(mini_os_thread_t *thread, mini_os_uint32_t ticks)
{
    mini_os_uint32_t slot, round;

    if (thread == MINI_OS_NULL || ticks == 0u)
    {
        return MINI_OS_ERR_INVAL;
    }

    slot  = (s_current_slot + ticks) & MINI_OS_TICK_WHEEL_MASK;
    round = (ticks - 1u) >> (MINI_OS_CTZ(MINI_OS_TICK_WHEEL));

    thread->round = round;
    thread->wheel_slot = (mini_os_uint8_t)slot;
    thread->state = MINI_OS_THREAD_STATE_BLOCKED;
    mini_os_list_tail(&thread->list_node, &s_wheel[slot]);
    return MINI_OS_OK;
}

mini_os_uint32_t mini_os_wheel_remain(mini_os_thread_t *thread)
{
    mini_os_uint32_t remain;

    if (thread == MINI_OS_NULL || thread->wheel_slot >= MINI_OS_TICK_WHEEL)
    {
        return 0u; /* not parked in the wheel (e.g. waiting on a sync object) */
    }
    remain = ((mini_os_uint32_t)thread->wheel_slot - s_current_slot) & MINI_OS_TICK_WHEEL_MASK;
    if (remain == 0u)
    {
        remain = MINI_OS_TICK_WHEEL; /* whole-wheel boundary (matches the -1 round formula) */
    }
    return remain + thread->round * MINI_OS_TICK_WHEEL;
}

void mini_os_schedule_delay(mini_os_uint32_t ticks)
{
    mini_os_irq_t irq_level;

    if (mini_os_current_thread == MINI_OS_NULL || ticks == 0u)
    {
        return;
    }
    irq_level = mini_os_irq_save();

    mini_os_remove_thread_from_ready_running_list(mini_os_current_thread);
    mini_os_wheel_insert(mini_os_current_thread, ticks);

    mini_os_irq_restore(irq_level);
    mini_os_schedule_yield();
}

#if MINI_OS_TIME_SLICE
/**
 * @brief Decrement the running thread's time slice; rotate when it expires
 * @note
 *  - remain_tick is the thread's own remaining quantum: it is decremented
 *    only while the thread is running and preserved across preemption/block
 *  - a thread without a configured slice (init_tick_num == 0) runs until it
 *    blocks or is preempted
 *  - on natural expiry the quantum is refilled for the thread's next run
 *    and the PendSV is triggered to rotate to the next same-priority thread
 */
static void mini_os_tick_slice_decrement(void)
{
    mini_os_thread_t *current_thread = mini_os_current_thread;

    if (current_thread == MINI_OS_NULL || current_thread->init_tick_num == 0)
    {
        return;
    }
    if (current_thread->remain_tick > 0)
    {
        current_thread->remain_tick--;
    }
    if (current_thread->remain_tick == 0)
    {
        current_thread->remain_tick = current_thread->init_tick_num; /* refill for the next run */
        mini_os_schedule_yield();
    }
}
#endif

void mini_os_systick_handler(void)
{
    mini_os_irq_t irq_level = mini_os_irq_save();
    mini_os_tick_decrement();
#if MINI_OS_TIME_SLICE
    mini_os_tick_slice_decrement();
#endif
    g_global_tick++;
#ifdef MINI_OS_LONG_TIME
    if (g_global_tick == 0u) /* wrapped around: count the overflow */
    {
        g_global_tick_overflow++;
    }
#endif
    mini_os_irq_restore(irq_level);
}

#ifdef MINI_OS_LONG_TIME
mini_os_err_t mini_os_get_tick_long_time(mini_os_uint32_t *tick, mini_os_uint32_t *overflow)
{
    *tick = g_global_tick;
    *overflow = g_global_tick_overflow;
    return MINI_OS_OK;
}
#endif

mini_os_err_t mini_os_get_tick(mini_os_uint32_t *tick)
{
    *tick = g_global_tick;
    return MINI_OS_OK;
}

MINI_OS_WEAK void mini_os_systick_init(uint32_t ticks_per_ms)
{
    uint32_t reload;

    if (ticks_per_ms == 0u)
    {
        ticks_per_ms = 1000u / MINI_OS_DEFAULT_SYSTICK; /* 0 -> default tick rate */
    }

    reload = (MINI_OS_CPU_CLOCK_HZ / 1000u) * ticks_per_ms; /* cycles per tick */


    MINI_OS_SYSTICK_RELOAD = reload - 1u;
    MINI_OS_SYSTICK_VAL = 0u;
    MINI_OS_SYSTICK_CTRL = MINI_OS_SYSTICK_CTRL_CLKSOURCE |MINI_OS_SYSTICK_CTRL_TICKINT |MINI_OS_SYSTICK_CTRL_ENABLE;
}
