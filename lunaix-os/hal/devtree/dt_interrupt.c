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

bool
parse_stdintr_prop(struct fdt_iter* it, struct dt_intr_node* node)
{
    if (propeq(it, "interrupt-parent")) {
        node->parent_hnd = __prop_getu32(it);
    }

    else if (propeq(it, "interrupt-extended")) {
        node->intr.extended = true;
        __mkprop_ptr(it, &node->intr.arr);
    }

    else if (!node->intr.extended && propeq(it, "interrupts")) {
        node->intr.valid = true;
        __mkprop_ptr(it, &node->intr.arr);
    }

    else {
        return false;
    }

    return true;
}