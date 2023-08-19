#ifndef __LUNAIX_INTC_H
#define __LUNAIX_INTC_H

#include <lunaix/types.h>

#define IRQ_TRIG_EDGE 0b0
#define IRQ_TRIG_LEVEL 0b1

#define IRQ_TYPE_FIXED (0b01 << 1)
#define IRQ_TYPE_NMI (0b11 << 1)
#define IRQ_TYPE (0b11 << 1)

#define IRQ_VE_HI (0b1 << 3)
#define IRQ_VE_LO (0b0 << 3)

#define IRQ_DEFAULT (IRQ_TRIG_EDGE | IRQ_TYPE_FIXED | IRQ_VE_HI)

struct intc_context
{
    char* name;
    void* data;

    void (*irq_attach)(struct intc_context*,
                       int irq,
                       int iv,
                       cpu_t dest,
                       u32_t flags);
    void (*notify_eoi)(struct intc_context*, cpu_t id, int iv);
};

void
intc_init();

void
intc_irq_attach(int irq, int iv, cpu_t dest, u32_t flags);

/**
 * @brief Notify end of interrupt event
 *
 * @param id
 */
void
intc_notify_eoi(cpu_t id, int iv);

/**
 * @brief Notify end of scheduling event
 *
 * @param id
 */
void
intc_notify_eos(cpu_t id);

#endif /* __LUNAIX_INTC_H */
