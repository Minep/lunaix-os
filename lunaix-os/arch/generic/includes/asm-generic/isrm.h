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
#include <lunaix/device.h>

#include <hal/devtree.h>

typedef void (*isr_cb)(const struct hart_state*);

void
isrm_init();

/**
 * @brief Notify end of interrupt event
 *
 * @param id
 */
void
isrm_notify_eoi(cpu_t id, int iv);

#endif /* __LUNAIX_ISRM_H */
