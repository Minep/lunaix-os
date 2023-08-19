#include <hal/intc.h>
#include <lunaix/spike.h>

#include <sys/vectors.h>

struct intc_context arch_intc_ctx;

void
intc_irq_attach(int irq, int iv, cpu_t dest, u32_t flags)
{
    arch_intc_ctx.irq_attach(&arch_intc_ctx, irq, iv, dest, flags);
}

void
intc_notify_eoi(cpu_t id, int iv)
{
    arch_intc_ctx.notify_eoi(&arch_intc_ctx, id, iv);
}

void
intc_notify_eos(cpu_t id)
{
    intc_notify_eoi(id, LUNAIX_SCHED);
}