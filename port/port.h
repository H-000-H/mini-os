/**
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file port.h
 * @brief export symbols for port layer
 * @author H-000-H
 */
#ifndef PORT_H
#define PORT_H
#ifdef __cplusplus
extern "C" {
#endif
#include "redef.h"
typedef void (*svc_call_back)(mini_os_uint32_t *frame, void *arg);

/* SVC helpers */
void mini_os_svc_set_callback(svc_call_back cb, void *arg);

mini_os_uint8_t mini_os_svc_get_num(mini_os_uint32_t *frame);

void mini_os_svc_dispatch(mini_os_uint32_t *frame, svc_call_back cb, void *arg);

/* Assembly entry points (naked, defined in port.c) */
void pendsv_handler(void);

void svc_handler(void);

void mini_os_start_first_thread(void);

void mini_os_yield_trigger(void);

mini_os_err_t mini_os_nvic_set_priority(mini_os_uint32_t irq, mini_os_uint32_t priority);

void mini_os_psp_set(mini_os_uint32_t psp);

void mini_os_set_control(mini_os_uint32_t control);

#ifdef __cplusplus
}
#endif
#endif /* PORT_H */
