#include <arch/x86/interrupts.h>
#include <hal/acpi/acpi.h>
#include <hal/ioapic.h>
#include <lunaix/common.h>
#include <lunaix/mm/mmio.h>

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
ioapic_redirect(u8_t irq, u8_t vector, u8_t dest, u32_t flags)
{
    u8_t reg_sel = IOAPIC_IOREDTBL_BASE + irq * 2;

    // Write low 32 bits
    ioapic_write(reg_sel, (vector | flags) & 0x1FFFF);

    // Write high 32 bits
    ioapic_write(reg_sel + 1, (dest << 24));
}