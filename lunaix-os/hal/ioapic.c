#include <arch/x86/interrupts.h>
#include <hal/ioapic.h>
#include <hal/acpi/acpi.h>


#define IOAPIC_REG_SEL     *((volatile uint32_t*)(IOAPIC_BASE_VADDR + IOAPIC_IOREGSEL))
#define IOAPIC_REG_WIN     *((volatile uint32_t*)(IOAPIC_BASE_VADDR + IOAPIC_IOWIN))

uint8_t
ioapic_get_irq(acpi_context* acpi_ctx, uint8_t old_irq);

void
ioapic_init() {
    // Remapping the IRQs
    
    acpi_context* acpi_ctx = acpi_get_context();

    // Remap the IRQ 8 (rtc timer's vector) to RTC_TIMER_IV in ioapic
    //       (Remarks IRQ 8 is pin INTIN8)
    //       See IBM PC/AT Technical Reference 1-10 for old RTC IRQ
    //       See Intel's Multiprocessor Specification for IRQ - IOAPIC INTIN mapping config.
    
    // The ioapic_get_irq is to make sure we capture those overriden IRQs

    // PC_AT_IRQ_RTC -> RTC_TIMER_IV, fixed, edge trigged, polarity=high, physical, APIC ID 0
    ioapic_redirect(ioapic_get_irq(acpi_ctx, PC_AT_IRQ_RTC), RTC_TIMER_IV, 0, IOAPIC_DELMOD_FIXED);
}

uint8_t
ioapic_get_irq(acpi_context* acpi_ctx, uint8_t old_irq) {
    if (old_irq >= 24) {
        return old_irq;
    }
    acpi_intso_t* int_override = acpi_ctx->madt.irq_exception[old_irq];
    return int_override ? (uint8_t)int_override->gsi : old_irq;
}

void
ioapic_write(uint8_t sel, uint32_t val) {
    IOAPIC_REG_SEL = sel;
    IOAPIC_REG_WIN = val;
}

uint32_t
ioapic_read(uint8_t sel) {
    IOAPIC_REG_SEL = sel;
    return IOAPIC_REG_WIN;
}

void
ioapic_redirect(uint8_t irq, uint8_t vector, uint8_t dest, uint32_t flags) {
    uint8_t reg_sel = IOAPIC_IOREDTBL_BASE + irq * 2;

    // Write low 32 bits
    ioapic_write(reg_sel, (vector | flags) & 0x1FFFF);

    // Write high 32 bits
    ioapic_write(reg_sel + 1, (dest << 24));
}