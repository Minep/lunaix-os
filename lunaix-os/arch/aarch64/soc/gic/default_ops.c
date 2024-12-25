#include "gic-common.h"

int 
__fallback_set_enabled(struct gic_int_class* class, intid_t intid, bool en)
{
    return 0;
}

int 
__fallback_set_prority(struct gic_int_class* class, intid_t intid, int prio)
{
    return 0;
}

int 
__fallback_set_nmi(struct gic_int_class* class, intid_t intid, bool nmi)
{
    return 0;
}

int 
__fallback_set_route(struct gic_int_class* class, 
                intid_t intid, struct gic_pe* target)
{
    return 0;
}

int 
__fallback_set_trigger(struct gic_int_class* class, 
                  intid_t intid, enum gic_trigger trig)
{
    return 0;
}


int 
__fallback_retrieve(struct gic_int_class* class, 
               struct gic_interrupt*, intid_t intid)
{
    return 0;
}

int 
__fallback_install(struct gic_int_class* class, struct gic_interrupt* gic)
{
    return 0;
}

int 
__fallback_delete(struct gic_int_class* class, intid_t intid)
{
    return class->ops->set_enabled(class, intid, false);
}