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

#include <lunaix/types.h>
#include <lunaix/hart_state.h>

typedef void (*isr_cb)(const struct hart_state*);

void
isrm_init();

/**
 * @brief Release a iv resource
 *
 * @param iv
 */
void
isrm_ivfree(int iv);

/**
 * @brief Allocate an iv resource for os services
 *
 * @param iv
 */
int
isrm_ivosalloc(isr_cb handler);

/**
 * @brief Allocate an iv resource for external events
 *
 * @param iv
 */
int
isrm_ivexalloc(isr_cb handler);

/**
 * @brief Bind a given irq and associated handler to an iv
 *
 * @param iv iv allocated by system
 */
int
isrm_bindirq(int irq, isr_cb irq_handler);

/**
 * @brief Bind given iv with it's associated handler
 *
 * @param iv
 * @param handler
 */
void
isrm_bindiv(int iv, isr_cb handler);

/**
 * @brief Get the handler associated with the given iv
 *
 * @param iv
 * @return isr_cb
 */
isr_cb
isrm_get(int iv);

ptr_t
isrm_get_payload(const struct hart_state*);

void
isrm_set_payload(int iv, ptr_t);

void
isrm_irq_attach(int irq, int iv, cpu_t dest, u32_t flags);

/**
 * @brief Notify end of interrupt event
 *
 * @param id
 */
void
isrm_notify_eoi(cpu_t id, int iv);

/**
 * @brief Notify end of scheduling event
 *
 * @param id
 */
void
isrm_notify_eos(cpu_t id);

#endif /* __LUNAIX_ISRM_H */
