#ifndef __LUNAIX_GIC_COMMON_H
#define __LUNAIX_GIC_COMMON_H

#include <lunaix/types.h>
#include <lunaix/status.h>
#include <hal/irq.h>

#define PAD(body, name, offset)   \
    struct { char _pad_##name[offset]; body; } compact

#define PAD_STRUCT(body, name, offset)  \
    PAD(struct body, name, offset)

#define FIELD_AT(type, field, offset)   \
    PAD(volatile type field, field, offset)

#define ARRAY_AT(type, arr, offset, end)   \
    PAD(volatile type arr[((end) - (offset)) / sizeof(type) + 1], arr, offset)


typedef unsigned int intid_t;

struct gic_pe;
struct gic_int_class;

enum gic_intkind
{
    GIC_INT_SGI = 0,
    GIC_INT_PPI,
    GIC_INT_PPI_EXT,
    GIC_INT_LPI,
    GIC_INT_SPI,
    GIC_INT_SPI_EXT,
    GIC_INT_COUNT
};

enum gic_intgrp
{
    GIC_INT_GROUP0,
    GIC_INT_GROUP1_S,
    GIC_INT_GROUP1_NS,
};

enum gic_trigger
{
    GIC_TRIG_LEVEL,
    GIC_TRIG_EDGE,
};

struct gic_interrupt
{
    intid_t intid;

    enum gic_intgrp grp;
    enum gic_intkind kind;
    enum gic_trigger trig;
    int priority;
    bool nmi;
    bool enabled;

    union {
        u64_t raw_val;
        struct gic_pe* pe;
    } affinity;
    
    struct irq_object* irq;
};
#define GIC_INTR_RAW_AFF    1

struct gic_int_class_ops
{
    int (*set_enabled)(struct gic_int_class*, intid_t, bool);
    int (*set_prority)(struct gic_int_class*, intid_t, int);
    int (*set_nmi)(struct gic_int_class*, intid_t, bool);
    int (*set_route)(struct gic_int_class*,intid_t, struct gic_pe*);
    int (*set_trigger)(struct gic_int_class*,intid_t, enum gic_trigger);

    int (*retrieve)(struct gic_int_class*, struct gic_interrupt*, intid_t);
    int (*install)(struct gic_int_class*, struct gic_interrupt*);
    int (*delete)(struct gic_int_class*, intid_t);

    int (*fire)(struct gic_int_class*, intid_t);
};
extern const struct gic_int_class_ops aa64_gic_fallback_ops;

struct gic;
struct gic_pe;
struct gic_int_class
{
    enum gic_intkind kind;
    
    struct {
        intid_t start;
        intid_t stop;
    } range;

    struct gic_int_class_ops* ops;
};

struct gic_pe_ops
{
    bool (*ack_int)(struct gic_pe*);
    int (*notify_eoi)(struct gic_pe*);
    int (*set_priority_mask)(struct gic_pe*, int);
};

struct gic_pe
{
    union {
        struct {
            struct gic_int_class sgi;
            struct gic_int_class ppi;
            struct gic_int_class ppi_e;
            struct gic_int_class lpi;
        };
        struct gic_int_class classes[4];
    };

    struct {
        union {
            intid_t active_id;
            struct gic_interrupt active_int;
        };
        bool has_active_int;
    };

    void* impl;

    int index;
    int affinity;

    struct gic* distr;
    struct gic_pe_ops* ops;
};

struct gic
{
    int nr_cpus;
    struct gic_pe** cores;

    union {
        struct {
            struct gic_int_class spi;
            struct gic_int_class spi_e;
        };
        struct gic_int_class classes[2];
    };

    void* impl;

    struct irq_domain* domain;
};

static inline void
gic_init_int_class(struct gic_int_class* class, 
                   enum gic_intkind kind, intid_t start, intid_t end)
{
    *class = (struct gic_int_class) {
        .range.start = start,
        .range.stop  = end,
        .kind = kind
    };
}

static inline bool
gic_valid_int_class(struct gic_int_class* class) {
    return class->ops && (class->range.start < class->range.stop);
}

static inline void
gic_intr_set_raw_affval(struct gic_interrupt* intr, u32_t affval)
{
    intr->affinity.raw_val = ((u64_t)affval << 1) | GIC_INTR_RAW_AFF;
}

static inline int
gic_assign_intid(struct gic_int_class* class, irq_t irq)
{
    int err;
    struct gic* gic;

    if (!gic_valid_int_class(class)) {
        // selected class not present or disabled
        return ENOENT;
    }

    gic = gic_global_context_of(class);
    err = irq_alloc_id(gic->domain, irq, 
                        class->range.start, class->range.stop);

    return err;
}

struct gic_pe*
gic_create_pe_context(struct gic* gic, void* pe_impl);

struct gic_pe*
gic_local_context_of(struct gic_int_class* int_class);

struct gic*
gic_global_context_of(struct gic_int_class* int_class);

int
gic_get_interrupt(struct gic* gic, intid_t id, struct gic_interrupt* intr_out);

struct gic*
gic_create_context(struct device* gicdev);


// default ops

int 
__fallback_set_enabled(struct gic_int_class* class, intid_t intid, bool en);

int 
__fallback_set_prority(struct gic_int_class* class, intid_t intid, int prio);

int 
__fallback_set_nmi(struct gic_int_class* class, intid_t intid, bool nmi);

int 
__fallback_set_route(struct gic_int_class* class, 
                intid_t intid, struct gic_pe* target);

int 
__fallback_set_trigger(struct gic_int_class* class, 
                  intid_t intid, enum gic_trigger trig);


int 
__fallback_retrieve(struct gic_int_class* class, 
               struct gic_interrupt*, intid_t intid);

int 
__fallback_install(struct gic_int_class* class, struct gic_interrupt* gic);

int 
__fallback_delete(struct gic_int_class* class, intid_t intid);
#endif /* __LUNAIX_GIC_COMMON_H */
