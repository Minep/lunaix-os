#ifndef __LUNAIX_IOAPIC_H
#define __LUNAIX_IOAPIC_H

#include "sys/x86_isa.h"

void
ioapic_init();

void
ioapic_irq_remap(struct x86_intc*,
                 int irq,
                 int iv,
                 cpu_t dest,
                 u32_t flags);

#endif /* __LUNAIX_IOAPIC_H */
