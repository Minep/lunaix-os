#ifndef __LUNAIX_IRQ_H
#define __LUNAIX_IRQ_H

#include <lunaix/changeling.h>
#include <lunaix/device.h>
#include <lunaix/ds/btrie.h>
#include <lunaix/status.h>
#include <asm/hart.h>

#define IRQ_VECTOR_UNSET    ((unsigned)-1)

struct irq_domain;
typedef struct irq_object* irq_t;
typedef void (*irq_servant)(irq_t, const struct hart_state*);

struct irq_domain_ops
{
    int
    (*install_irq)(struct irq_domain*, irq_t);

    int
    (*map_irq)(struct irq_domain*, irq_t, void* irq_extra);

    int
    (*remove_irq)(struct irq_domain*, irq_t);
};

struct irq_domain
{
    POTENS_META;

    struct llist_header list;

    struct irq_domain* parent;
    struct btrie irq_map;
    const struct irq_domain_ops* ops;
    void* object;
};

enum irq_type
{
    IRQ_DIRECT,
    IRQ_LINE,
    IRQ_MESSAGE
};

enum irq_trigger
{
    IRQ_EDGE = 0,
    IRQ_LEVEL
};

struct irq_line_wire
{
    ptr_t domain_local;
    ptr_t domain_mapped;
};

struct irq_msi_wire
{
    ptr_t message;
    ptr_t sideband;
    ptr_t wr_addr;
};

struct irq_object
{    
    enum irq_type type;
    enum irq_trigger trig;
    unsigned int vector;
    
    union {
        struct irq_line_wire* line;
        struct irq_msi_wire* msi;
    };

    irq_servant serve;
    void* payload;

    struct irq_domain* domain;
    int ref;
};

struct irq_domain*
irq_create_domain(struct device* intc_dev, const struct irq_domain_ops* ops);

struct irq_domain*
irq_owning_domain(struct device* dev);

int
irq_attach_domain(struct irq_domain* parent, struct irq_domain* child);

irq_t
irq_declare(enum irq_type, irq_servant, ptr_t);

void
irq_revoke(irq_t);

int
irq_assign(struct irq_domain* domain, irq_t, void*);

irq_t
irq_find(struct irq_domain* domain, int local_irq);

void
irq_record(struct irq_domain* domain, irq_t irq);

void
irq_set_default_domain(struct irq_domain*);

struct irq_domain*
irq_get_default_domain();

int
irq_forward_install(struct irq_domain* current, irq_t irq);

static inline int
irq_alloc_id(struct irq_domain* domain, irq_t irq, int start, int end)
{
    unsigned int irq_id;

    if (irq->vector != IRQ_VECTOR_UNSET) {
        return EEXIST;
    }
    
    irq_id = (unsigned int)btrie_map(&domain->irq_map, start, end, irq);
    if (irq_id == -1U) {
        return E2BIG;
    }

    irq->vector = irq;
    return 0;
}

static inline void
irq_serve(irq_t irq, struct hart_state* state)
{
    irq->serve(irq, state);
}

static inline void
irq_set_servant(irq_t irq, irq_servant callback)
{
    irq->serve = callback;
}

static inline void
irq_bind_vector(irq_t irq, int vector)
{
    irq->vector = vector;
}

static inline void
irq_set_payload(irq_t irq, void* payload)
{
    irq->payload = payload;
}

static inline void
irq_set_domain_object(struct irq_domain* domain, void* obj)
{
    domain->object = obj;
}

#define irq_payload(irq, type)              ((type*)(irq)->payload)
#define irq_domain_obj(domain, type)        ((type*)(domain)->object)

static inline irq_t
irq_declare_line(irq_servant callback, int local_irq)
{
    return irq_declare(IRQ_LINE, callback, (int)local_irq);
}

static inline irq_t
irq_declare_msg(irq_servant callback, 
                ptr_t message, ptr_t sideband)
{
    irq_t irq;
    irq = irq_declare(IRQ_MESSAGE, callback, message);
    irq->msi->sideband = sideband;

    return irq;
}

static inline irq_t
irq_declare_direct(irq_servant callback)
{
    return irq_declare(IRQ_DIRECT, callback, 0);
}

static inline struct irq_domain*
irq_get_domain(struct device* maybe_intc)
{
    struct potens_meta* domain_m;

    domain_m = device_get_potens(maybe_intc, potens(INT_DOMAIN));
    return domain_m ? get_potens(domain_m, struct irq_domain) : NULL;
}

static inline struct irq_domain*
irq_parent_domain(struct irq_domain* domain)
{
    return domain->parent;
}

#endif /* __LUNAIX_IRQ_H */
