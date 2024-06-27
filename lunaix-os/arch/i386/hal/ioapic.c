#include <hal/acpi/acpi.h>

#include <lunaix/mm/mmio.h>

#include <sys/hart.h>
#include "sys/ioapic.h"
#include "sys/x86_isa.h"

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

#define IOAPIC_REG_SEL *((volatile u32_t*)(_ioapic_base + IOAPIC_IOREGSEL))
#define IOAPIC_REG_WIN *((volatile u32_t*)(_ioapic_base + IOAPIC_IOWIN))

static volatile ptr_t _ioapic_base;

void
ioapic_init()
{
    // Remapping the IRQs

    acpi_context* acpi_ctx = acpi_get_context();

    _ioapic_base =
      (ptr_t)ioremap(acpi_ctx->madt.ioapic->ioapic_addr & ~0xfff, 4096);
}

void
ioapic_write(u8_t sel, u32_t val)
{
    IOAPIC_REG_SEL = sel;
    IOAPIC_REG_WIN = val;
}

u32_t
ioapic_read(u8_t sel)
{
    IOAPIC_REG_SEL = sel;
    return IOAPIC_REG_WIN;
}

void
ioapic_irq_remap(struct x86_intc* intc, int irq, int iv, cpu_t dest, u32_t flags)
{
    /*
        FIXME move it to HAL level. since every platform might have their own
       wiring, thus gsi mapping is required all the time
    */
    irq = acpi_gsimap(irq);
    u8_t reg_sel = IOAPIC_IOREDTBL_BASE + irq * 2;

    u32_t ioapic_fg = 0;

    if ((flags & IRQ_TYPE) == IRQ_TYPE_FIXED) {
        ioapic_fg |= IOAPIC_DELMOD_FIXED;
    } else {
        ioapic_fg |= IOAPIC_DELMOD_NMI;
    }

    if ((flags & IRQ_TRIG_LEVEL)) {
        ioapic_fg |= IOAPIC_TRIG_LEVEL;
    }

    if (!(flags & IRQ_VE_HI)) {
        ioapic_fg |= IOAPIC_INTPOL_L;
    }

    // Write low 32 bits
    ioapic_write(reg_sel, (iv | ioapic_fg) & 0x1FFFF);

    // Write high 32 bits
    ioapic_write(reg_sel + 1, (dest << 24));
}