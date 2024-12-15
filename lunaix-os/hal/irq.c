#include <hal/irq.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/owloysius.h>

static struct irq_domain* default_domain = NULL;
static DEFINE_LLIST(irq_domains);

static void
__default_servant(irq_t irq, const struct hart_state* state)
{
    return;
}

struct irq_domain*
irq_create_domain(struct device* intc_dev, const struct irq_domain_ops* ops)
{
    struct irq_domain* domain;

    assert(ops->install_irq);

    domain = new_potens(potens(INT_DOMAIN), struct irq_domain);
    device_grant_potens(intc_dev, potens_meta(domain));

    btrie_init(&domain->irq_map, ilog2(8));
    llist_append(&irq_domains, &domain->list);
    domain->ops = ops;
    
    return domain;
}

struct irq_domain*
irq_owning_domain(struct device* dev)
{
#ifdef CONFIG_USE_DEVICETREE
    struct device* intc;
    struct dtn* intr_parent;
    struct potens_meta* domain_m;

    intr_parent = dev->devtree_node->intr.parent;
    intc = resolve_device_morph(dt_mobj(intr_parent));

    if (!intc) {
        return NULL;
    }

    domain_m = device_get_potens(intc, potens(INT_DOMAIN));
    return !domain_m ?: get_potens(domain_m, struct irq_domain);
#else
    return default_domain;
#endif
}

int
irq_attach_domain(struct irq_domain* parent, struct irq_domain* child)
{
#ifndef CONFIG_USE_DEVICETREE
    parent = parent ?: default_domain;
    child->parent = parent;
#endif
    return 0;
}

static void
__irq_create_line(irq_t irq, ptr_t local_int)
{
    struct irq_line_wire *line;

    line = valloc(sizeof(*line));
    line->domain_local = local_int;
    line->domain_mapped = local_int;

    irq->line = line;
}

static void
__irq_create_msi(irq_t irq, ptr_t message)
{
    struct irq_msi_wire *msi;

    msi = valloc(sizeof(*msi));
    msi->message  = message;

    irq->msi = msi;
}

irq_t
irq_declare(enum irq_type type, irq_servant callback, ptr_t data)
{
    irq_t irq;

    irq  = valloc(sizeof(*irq));
    *irq = (struct irq_object) {
        .type = type,
        .serve = callback ?: __default_servant,
        .vector = IRQ_VECTOR_UNSET
    };

    if (type == IRQ_LINE) {
        __irq_create_line(irq, data);
    }

    else if (type == IRQ_MESSAGE) {
        __irq_create_msi(irq, data);
    }

    return irq;
}

void
irq_revoke(irq_t irq)
{
    if (--(irq->ref) > 0) {
        return;
    }
    vfree(irq);
}

int
irq_assign(struct irq_domain* domain, irq_t irq, void* irq_spec)
{
    int err = 0;    
    if (domain->ops->map_irq) {
        err = domain->ops->map_irq(domain, irq, irq_spec);
        if (err) {
            return err;
        }
    }

    /*
        A domain controller may choose to forward the interrupt
        (i.e., irq became transparent and belongs to the higher domain)
        We allow controller decide whether to record the irq under its wing
    */
    err = domain->ops->install_irq(domain, irq);
    if (err) {
        return err;
    }

    irq->domain = domain;
    return 0;
}

irq_t
irq_find(struct irq_domain* domain, int local_irq)
{
    irq_t irq = NULL;
    struct irq_domain* current;

    // Find recursively, in case of irq forwarding

    current = domain ?: default_domain;
    while (current && !irq) {
        irq = (irq_t)btrie_get(&current->irq_map, local_irq);
        current = irq_parent_domain(current);
    }
    
    return irq;
}

void
irq_record(struct irq_domain* domain, irq_t irq)
{
    irq->ref++;
    btrie_set(&domain->irq_map, irq->vector, irq);
}

void
irq_set_default_domain(struct irq_domain* domain)
{
    assert(!default_domain);
    default_domain = domain;
}

int
irq_forward_install(struct irq_domain* current, irq_t irq)
{
    struct irq_domain* parent;

    parent = irq_parent_domain(current);

    if (irq->type == IRQ_LINE) {
        irq->line->domain_local = irq->line->domain_mapped;
    }

    irq->domain = parent;
    return parent->ops->install_irq(parent, irq);
}

struct irq_domain*
irq_get_default_domain()
{
    assert(default_domain);
    return default_domain;
}
