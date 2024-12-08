#ifndef __LUNAIX_X86_ISRM_H
#define __LUNAIX_X86_ISRM_H

#include <asm-generic/isrm.h>

/**
 * @brief Bind given iv with it's associated handler
 *
 * @param iv
 * @param handler
 */
void
isrm_bindiv(int iv, isr_cb handler);

void
isrm_irq_attach(int irq, int iv, cpu_t dest, u32_t flags);

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
 * @brief Release a iv resource
 *
 * @param iv
 */
void
isrm_ivfree(int iv);

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


#endif /* __LUNAIX_X86_ISRM_H */
