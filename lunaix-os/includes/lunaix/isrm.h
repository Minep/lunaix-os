/**
 * @file irqm.h
 * @author Lunaixsky
 * @brief ISR Manager, managing the interrupt service routine allocations
 * @version 0.1
 * @date 2022-10-18
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __LUNAIX_ISRM_H
#define __LUNAIX_ISRM_H

#include <arch/x86/interrupts.h>
#include <lunaix/types.h>

#define IV_BASE 32
#define IV_OS IV_BASE
#define IV_EX 48
#define IV_MAX 256

typedef void (*isr_cb)(const isr_param*);

void
isrm_init();

void
isrm_ivfree(int iv);

int
isrm_ivosalloc(isr_cb handler);

int
isrm_ivexalloc(isr_cb handler);

int
isrm_bindirq(int irq, isr_cb irq_handler);

int
isrm_bindiv(int iv, isr_cb handler);

isr_cb
isrm_get(int iv);

#endif /* __LUNAIX_ISRM_H */
