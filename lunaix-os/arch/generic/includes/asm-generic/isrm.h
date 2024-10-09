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

#include <hal/devtree.h>

typedef void (*isr_cb)(const struct hart_state*);

typedef struct {
    ptr_t msi_addr;
    reg_t msi_data;
    int mapped_iv;
} msi_vector_t;
#define msi_addr(msiv)   ((msiv).msi_addr)
#define msi_data(msiv)   ((msiv).msi_data)
#define msi_vect(msiv)   ((msiv).mapped_iv)
#define check_msiv_invalid(msiv)  (msi_vect(msiv) == -1)
#define invalid_msi_vector  ((msi_vector_t) { (ptr_t)-1, (reg_t)-1, -1 });

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
 * @brief Allocate an iv resource for MSI use
 *
 * @param iv
 */
msi_vector_t
isrm_msialloc(isr_cb handler);

/**
 * @brief Bind the iv according to given device tree node
 *
 * @param node
 */
int
isrm_bind_dtnode(struct dt_intr_node* node, isr_cb handler);

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
