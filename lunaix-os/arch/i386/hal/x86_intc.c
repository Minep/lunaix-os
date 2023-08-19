#include <hal/intc.h>
#include <sys/apic.h>
#include <sys/ioapic.h>

extern struct intc_context arch_intc_ctx;

void
intc_init()
{
    apic_init();
    ioapic_init();

    arch_intc_ctx.name = "i386_apic";
    arch_intc_ctx.irq_attach = ioapic_irq_remap;
    arch_intc_ctx.notify_eoi = apic_on_eoi;
}
