#ifndef __LUNAIX_X86_ISRM_H
#define __LUNAIX_X86_ISRM_H

#include <asm-generic/isrm.h>

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

#endif /* __LUNAIX_X86_ISRM_H */
