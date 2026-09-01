@ SPDX-License-Identifier: Apache-2.0
@ file port.S
@ Cortex-M port: PendSV context switch, first-thread start, yield trigger, SVC handler
@ Uppercase .S: preprocessed by CPP (headers/macros may be included if needed).
@ The stack frame layout matches mini_os_thread_stack_init in thread.c:
@ low -> high: r4..r11, R0..R3, R12, LR, PC, xPSR.
#include "mini_config.h"
    .syntax unified
    .thumb

    .extern mini_os_current_thread
    .extern mini_os_schedule_switch
    .extern g_svc_cb
    .extern g_svc_arg
    .extern mini_os_svc_dispatch
    .extern mini_os_yield_trigger
    .equ    MINI_OS_EXC_RETURN_THREAD_PSP, 0xFFFFFFFD  @ EXC_RETURN: return to Thread mode, use PSP
    .equ    NVIC_ICSR,                      0xE000ED04  @ Interrupt Control and State Register
    .equ    NVIC_PENDSVSET,                 (1u<<28)  @ PendSV set-pending bit
    .equ    NVIC_PENDSVCLR,                 (1u<<27)  @ PendSV clear-pending bit
    .equ    NVIC_IPR0,                      0xE000E400  @ NVIC Interrupt Priority Register base (one byte per IRQ)
#if MINI_OS_ARCH == MINI_OS_ARCH_M0
    .equ    MINI_OS_NVIC_PRIO_BITS,         2           @ Cortex-M0/M0+: 2 (adjust per core)
#else
    .equ    MINI_OS_NVIC_PRIO_BITS,         4           @ Cortex-M3/M4: 4; Cortex-M0/M0+: 2 (adjust per core)
#endif
    .equ    SYSTICK_CTRL,                   0xE000E010  @ SysTick Control and Status Register
    .text

@-----------------------------------------------------------------------------
@ pendsv_handler : context switch exception handler
@-----------------------------------------------------------------------------
    .global pendsv_handler
    .thumb_func
    .type pendsv_handler, %function
pendsv_handler:
    cpsid   i                       @ disable interrupts
    mrs     r0, psp                 @ r0 = current task PSP

    ldr     r2, =mini_os_current_thread
    ldr     r1, [r2]                @ r1 = current TCB pointer

    cmp     r1, #0
    beq     .Lskip_save             @ current thread is NULL, skip saving

    stmdb   r0!, {r4-r11}           @ save callee-saved registers r4-r11
    str     r0, [r1]                @ TCB->sp = r0

.Lskip_save:
    push    {r2, lr}                @ keep r2 (symbol address) and LR for the C call
    bl      mini_os_schedule_switch
    pop     {r2, lr}

    ldr     r1, [r2]                @ r1 = new TCB pointer
    ldr     r0, [r1]                @ r0 = new TCB->sp

    ldmia   r0!, {r4-r11}           @ restore callee-saved registers
    msr     psp, r0
    isb

    cpsie   i
    bx      lr                      @ exception return: hardware pops r0-r3, r12, lr, pc, xpsr

@-----------------------------------------------------------------------------
@ mini_os_start_first_thread : switch from MSP to PSP, enter the first thread
@-----------------------------------------------------------------------------
    .global mini_os_start_first_thread
    .thumb_func
    .type mini_os_start_first_thread, %function
mini_os_start_first_thread:
    ldr     r2, =mini_os_current_thread
    ldr     r0, [r2]                @ r0 = TCB pointer
    ldr     r0, [r0]                @ r0 = TCB->sp

    ldmia   r0!, {r4-r11}           @ restore callee-saved registers
    msr     psp, r0
    isb

    ldr     lr, =MINI_OS_EXC_RETURN_THREAD_PSP @ EXC_RETURN: return to thread mode, use PSP
    cpsie   i
    bx      lr

@-----------------------------------------------------------------------------
@ mini_os_yield_trigger : trigger a context switch from C (set PendSV pending)
@-----------------------------------------------------------------------------
    .global mini_os_yield_trigger
    .thumb_func
    .type mini_os_yield_trigger, %function
mini_os_yield_trigger:
    ldr     r0, =NVIC_ICSR           @ Interrupt Control and State Register
    ldr     r1, =NVIC_PENDSVSET      @ PendSV set-pending bit
    str     r1, [r0]
    dsb
    isb
    movs    r0, #0
    bx      lr

    .global mini_os_cancel_pendsv
    .thumb_func
    .type mini_os_cancel_pendsv, %function
mini_os_cancel_pendsv:
    ldr     r0, =NVIC_ICSR           @ Interrupt Control and State Register
    ldr     r1, =NVIC_PENDSVCLR      @ PendSV clear-pending bit
    str     r1, [r0]
    dsb
    isb
    movs    r0, #0
    bx      lr

@-----------------------------------------------------------------------------
@ Set the priority of an NVIC interrupt
@ mini_os_nvic_set_priority(irq_type, priority) return MINI_OS_OK
@-----------------------------------------------------------------------------
    .global mini_os_nvic_set_priority
    .thumb_func
    .type mini_os_nvic_set_priority, %function
mini_os_nvic_set_priority:
    @ r0 = IRQ number, r1 = priority (0 = highest)
    ldr     r2, =NVIC_IPR0           @ NVIC interrupt priority register base
    add     r2, r0                   @ one priority byte per IRQ
    lsl     r1, r1, #(8 - MINI_OS_NVIC_PRIO_BITS) @ priority lives in the top bits of the byte
    strb    r1, [r2]                 @ write the priority byte
    dsb
    isb
    movs    r0, #0                   @ return MINI_OS_OK
    bx      lr

    .global mini_os_psp_set
    .thumb_func
    .type mini_os_psp_set, %function

@-----------------------------------------------------------------------------
@ mini_os_psp_set : set the PSP to the given value  return void
@-----------------------------------------------------------------------------
    .global mini_os_psp_set
    .thumb_func
    .type mini_os_psp_set, %function
mini_os_psp_set:
    msr     psp, r0

@-----------------------------------------------------------------------------
@ mini_os_set_control : set the control register to the given value  return void
@-----------------------------------------------------------------------------
    .global mini_os_set_control
    .thumb_func
    .type mini_os_set_control, %function
mini_os_set_control:
    msr     control, r0
    isb

@-----------------------------------------------------------------------------
@ svc_handler : pass the exception frame, callback and arg to the C dispatcher
@-----------------------------------------------------------------------------
    .global svc_handler
    .thumb_func
    .type svc_handler, %function
svc_handler:
    tst     lr, #4                  @ EXC_RETURN bit2: 1 = PSP, 0 = MSP
    beq     .Luse_msp
    mrs     r0, psp                 @ r0 = exception frame
    b       .Lsvc_done
.Luse_msp:
    mrs     r0, msp
.Lsvc_done:
    ldr     r1, =g_svc_cb
    ldr     r1, [r1]                @ r1 = callback
    ldr     r2, =g_svc_arg
    ldr     r2, [r2]                @ r2 = arg
    bl      mini_os_svc_dispatch
    bx      lr                      @ exception return
