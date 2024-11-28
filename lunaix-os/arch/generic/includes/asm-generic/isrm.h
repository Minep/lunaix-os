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

typedef struct {
    ptr_t msi_addr;
    reg_t msi_data;
    int mapped_iv;
} msi_vector_t;
#define msi_addr(msiv)   ((msiv).msi_addr)
#define msi_data(msiv)   ((msiv).msi_data)
#define msi_vect(msiv)   ((msiv).mapped_iv)

typedef void* msienv_t;

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
 * @brief Begin MSI allocation for given device
 *
 * @param iv
 */
msienv_t
isrm_msi_start(struct device* dev);

/**
 * @brief Query number of msi avaliable for the device
 */
int
isrm_msi_avaliable(msienv_t msienv);

/**
 * @brief Allocate a msi resource within defined msi resource list
 *        for the device, indexed by `index`
 */
msi_vector_t
isrm_msi_alloc(msienv_t msienv, cpu_t cpu, int index, isr_cb handler);

/**
 * @brief Set the sideband information will be used for upcoming
 *        allocations
 */
void
isrm_msi_set_sideband(msienv_t msienv, ptr_t sideband);

/**
 * @brief Done MSI allocation
 */
void
isrm_msi_done(msienv_t msienv);

static inline must_inline msi_vector_t
isrm_msi_alloc_simple(struct device* dev, cpu_t cpu, isr_cb handler)
{   
    msi_vector_t v;
    msienv_t env;

    env = isrm_msi_start(dev);
    v = isrm_msi_alloc(env, cpu, 0, handler);
    isrm_msi_done(env);

    return v;
}

/**
 * @brief Bind the iv according to given device tree node
 *
 * @param node
 */
int
isrm_bind_dtn(struct dtn_intr* node);

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
