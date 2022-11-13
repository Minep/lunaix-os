#ifndef __LUNAIX_IOAPIC_H
#define __LUNAIX_IOAPIC_H

#include <stdint.h>

#define IOAPIC_IOREGSEL 0x00
#define IOAPIC_IOWIN 0x10
#define IOAPIC_IOREDTBL_BASE 0x10

#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_ARB 0x02

#define IOAPIC_DELMOD_FIXED 0b000
#define IOAPIC_DELMOD_LPRIO 0b001
#define IOAPIC_DELMOD_NMI 0b100

#define IOAPIC_MASKED (1 << 16)
#define IOAPIC_TRIG_LEVEL (1 << 15)
#define IOAPIC_INTPOL_L (1 << 13)
#define IOAPIC_DESTMOD_LOGIC (1 << 11)

#define IOAPIC_BASE_VADDR 0x2000

void
ioapic_init();

void
ioapic_write(uint8_t sel, u32_t val);

u32_t
ioapic_read(uint8_t sel);

void
ioapic_redirect(uint8_t irq, uint8_t vector, uint8_t dest, u32_t flags);

#endif /* __LUNAIX_IOAPIC_H */
