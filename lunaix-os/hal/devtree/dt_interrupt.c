#include "devtree.h"

#include <lunaix/mm/valloc.h>
#include <klibc/string.h>

static int 
__ispec_sz(struct dtn* node)
{
    return node->base.intr_c + node->base.addr_c;
}

static struct dtp_val*
__ispec_mask(struct dtn* node)
{
    return dt_getprop(&node->base, "interrupt-map-mask");
}

void
dt_resolve_interrupt_map(struct dtn* node)
{
    struct dtp_val* map;
    struct dtspec_create_ops ops = {};
    
    ops.child_keysz = __ispec_sz;
    ops.parent_keysz = __ispec_sz;
    ops.get_mask = __ispec_mask;
    
    map = dt_getprop(&node->base, "interrupt-map");
    if (likely(!map)) {
        return;
    }

    node->intr.map = dtspec_create(node, map, &ops);
}

struct dtn* 
dt_interrupt_at(struct dtn* node, int idx, struct dtp_val* int_spec)
{
    int int_cells;
    dt_enc_t raw_specs;
    struct dtn_intr* intr;
    struct dtn* intr_domain;

    intr = &node->intr;
    if (!intr->valid || idx >= intr->nr_intrs) {
        return NULL;
    }
    
    if (!intr->extended) {
        intr_domain = intr->parent;
        int_cells = intr_domain->base.intr_c;
        
        raw_specs = &intr->raw_ispecs.encoded[int_cells * idx];
        dtp_val_set(int_spec, raw_specs, int_cells);

        return intr_domain;
    }

    struct dtspec_intr *p, *n;
    llist_for_each(p, n, &intr->ext_ispecs, ispecs)
    {
        if (!(idx--)) {
            *int_spec = p->val;
            return p->domain;
        }
    }

    return NULL;
}

bool
parse_stdintr_prop(struct fdt_blob* fdt, fdt_loc_t loc, struct dtn_intr* node)
{
    if (propeq(fdt, loc, "interrupt-parent")) {
        node->parent_hnd = __prop_getu32(loc);
    }

    else if (propeq(fdt, loc, "interrupts-extended")) {
        node->extended = true;
        __mkprop_ptr(loc, &node->raw_ispecs);
    }

    else if (!node->extended && propeq(fdt, loc, "interrupts")) {
        __mkprop_ptr(loc, &node->raw_ispecs);
    }

    else {
        return false;
    }

    node->valid = true;
    return true;
}