#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

#include "gic-common.h"

LOG_MODULE("gic")

// FUTURE for now we assume uniprocessor
const int current_cpu = 0;

struct gic_pe*
gic_create_pe_context(struct gic* gic, void* pe_impl)
{
    struct gic_pe* pe;

    pe = vzalloc(sizeof(*pe));
    
    pe->impl = pe_impl;
    pe->distr = gic;

    return pe;
}

struct gic_pe*
gic_local_context_of(struct gic_int_class* int_class)
{
    switch (int_class->kind)
    {
    case GIC_INT_PPI:
        return container_of(int_class, struct gic_pe, ppi);
    
    case GIC_INT_PPI_EXT:
        return container_of(int_class, struct gic_pe, ppi_e);

    case GIC_INT_SGI:
        return container_of(int_class, struct gic_pe, sgi);

    case GIC_INT_LPI:
        return container_of(int_class, struct gic_pe, lpi);

    default:
        return NULL;
    }
}

struct gic*
gic_global_context_of(struct gic_int_class* int_class)
{
    struct gic_pe* pe;

    switch (int_class->kind)
    {
    case GIC_INT_SPI:
        return container_of(int_class, struct gic, spi);
    
    case GIC_INT_SPI_EXT:
        return container_of(int_class, struct gic, spi_e);

    default:
        break;
    }

    pe = gic_local_context_of(int_class);

    return pe ? pe->distr : NULL;
}

static inline int
gic_resolve_pe(struct gic* gic, struct gic_interrupt* intr_out)
{
    u32_t affval;
    struct gic_pe* pe;

    if (!(intr_out->affinity.raw_val & GIC_INTR_RAW_AFF)) {
        return 0;
    }

    affval = (u32_t)(intr_out->affinity.raw_val >> 1);
    for (int i = 0; i < gic->nr_cpus; i++, pe = NULL)
    {
        pe = gic->cores[i];
        if (pe && pe->affinity == affval) {
            break;
        }
    }
    
    if (!pe) 
        return EINVAL;

    intr_out->affinity.pe = pe;

    return 0;
}

static inline bool
__intclass_inrange(struct gic_int_class* int_class, intid_t id)
{
    return int_class->ops 
        && (int_class->range.start <= id && id <= int_class->range.stop);
}

static inline enum gic_intkind
__get_intid_kind(struct gic* gic, intid_t id)
{
    struct gic_pe* pe;

    pe = gic->cores[current_cpu];
    if (__intclass_inrang(&gic->spi, id)) {
        return GIC_INT_SPI;
    }

    if (__intclass_inrang(&gic->spi_e, id)) {
        return GIC_INT_SPI_EXT;
    }

    if (__intclass_inrang(&pe->sgi, id)) {
        return GIC_INT_SGI;
    }

    if (__intclass_inrang(&pe->ppi, id)) {
        return GIC_INT_PPI;
    }

    if (__intclass_inrang(&pe->ppi_e, id)) {
        return GIC_INT_PPI_EXT;
    }

    if (__intclass_inrang(&pe->lpi, id)) {
        return GIC_INT_LPI;
    }

    return GIC_INT_COUNT;
}

int
gic_get_interrupt(struct gic* gic, intid_t id, struct gic_interrupt* intr_out)
{
    int err;
    enum gic_intkind kind;
    struct gic_int_class* class;
    struct gic_pe* current_pe;

    current_pe = gic->cores[current_cpu];
    kind = __get_intid_kind(gic, id);

    if (!current_pe) {
        return EINVAL;
    }

    if (kind == GIC_INT_SPI || kind == GIC_INT_SPI_EXT) {
        class = &gic->classes[kind - GIC_INT_SPI];
    }
    else if (kind < GIC_INT_SPI) {
        class = &current_pe->classes[kind];
    }
    else {
        return EINVAL;
    }

    if (!gic_valid_int_class(class)) {
        return ENOENT;
    }

    intr_out->intid = id;
    intr_out->kind = kind;

    err = class->ops->retrieve(class, intr_out, id);
    if (err) 
        return err;

    err = gic_resolve_pe(gic, intr_out);
    if (err)
        return err;

    // TODO get irq object

    return 0;
}

static void
__gic_create_profile(struct gic_interrupt* gint, 
                     enum gic_intkind kind, irq_t irq)
{
    gint->kind = kind;
    gint->trig = irq->trig == IRQ_EDGE ? GIC_TRIG_EDGE : GIC_TRIG_LEVEL;
    gint->grp  = GIC_INT_GROUP1_NS;
    gint->priority = 255;
    gint->intid = irq->vector;
    gint->irq = irq;
}

static int
__gic_install_irq(struct irq_domain *domain, irq_t irq)
{
    int err;
    struct gic* gic;
    struct gic_pe* pe;
    struct gic_interrupt gint;
    struct gic_int_class* class;

    gic = irq_domain_obj(domain, struct gic);
    pe  = gic->cores[current_cpu];

    switch (irq->type)
    {
    case IRQ_DIRECT:
        WARN("core-local interrupt not supported");
        return ENOTSUP;

    case IRQ_LINE:
        class = &gic->spi;
        break;

    case IRQ_MESSAGE:
        class = &pe->lpi;
        break;
    
    default:
        return EINVAL;
    }

    err = gic_assign_intid(class, irq);
    if (err)
        return err;

    __gic_create_profile(&gint, class->kind, irq);
    
    err = class->ops->install(class, &gint);
    if (err)
        return err;

    return class->ops->set_enabled(class, gint.intid, true);
}

static const struct irq_domain_ops gic_domain_ops = {
    .install_irq = __gic_install_irq
};

struct gic*
gic_create_context(struct device* gicdev)
{
    struct gic* gic;
    struct irq_domain* domain;

    gic = valloc(sizeof(*gic));
    domain = irq_create_domain(gicdev, &gic_domain_ops);

    irq_set_default_domain(domain);
    irq_set_domain_object(gic, domain);
    gic->domain = domain;

    return gic;
}

int
gic_handle_irq(struct hart_state* hs)
{
    int err = 0;
    struct gic* gic;
    struct gic_pe* pe;
    struct irq_domain* domain;
    irq_t irq;

    domain = irq_get_default_domain();
    gic = irq_domain_obj(domain, struct gic);

    pe = gic->cores[current_cpu];
    if (!pe->ops->ack_int(pe)) {
        return ENOENT;
    }

    err = gic_get_interrupt(gic, pe->active_id, &pe->active_int);
    if (err) {
        return err;
    }

    irq = pe->active_int.irq;
    assert(irq);

    irq->serve(irq, hs);

    pe->has_active_int = false;
    return pe->ops->notify_eoi(pe);
}