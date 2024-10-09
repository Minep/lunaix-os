#include <lunaix/types.h>
#include <asm/soc/gic.h>

#include <klibc/string.h>

static bool
__gic_predicate(struct dt_node_iter* iter, struct dt_node_base* pos)
{
    if (likely(!pos->intr_controll)) {
        return false;
    }

    return strneq(pos->compat.str_val, "arm,gic-", 8);
}

static bool
__its_predicate(struct dt_node_iter* iter, struct dt_node_base* pos)
{
    return strneq(pos->compat.str_val, "arm,gic-v3-its", 14);
}

static void
__setup_pe_rdist(struct arm_gic* gic, struct dt_prop_iter* prop)
{
    ptr_t base;
    size_t len, off;
    int i;

    base = dtprop_reg_nextaddr(prop);
    len  = dtprop_reg_nextlen(prop);

    assert(len >= NR_CPU * FRAME_SIZE * 2);

    i = 0;
    base = ioremap(base, len);
    off = base;

    for (; i < NR_CPU; i++) {
        gic->pes[i]._rd = (struct gic_rd*) (base + off);
        off += sizeof(struct gic_rd);
    }
}

static void
__create_its(struct arm_gic* gic, struct dt_node* gic_node)
{
    struct dt_node* its_node;
    struct dt_node_iter iter;
    struct dt_prop_iter prop;
    ptr_t its_base;
    size_t its_size;

    dt_begin_find(&iter, gic_node, __its_predicate, NULL);

    if (!dt_find_next(&iter, (struct dt_node_base**)&its_node)) {
        return;
    }

    dt_end_find(&iter);

    dt_decode_reg(&prop, its_node, reg);

    its_base = dtprop_reg_nextaddr(&prop);
    its_size = dtprop_reg_nextlen(&prop);

    assert(its_size >= sizeof(struct gic_its));

    gic->mmrs.its = (struct gic_its*)ioremap(its_base, its_size);
}

void
gic_create_from_dt(struct arm_gic* gic)
{
    struct dt_node* gic_node;
    struct dt_node_iter iter;
    struct dt_prop_iter prop;
    ptr_t ptr;
    size_t sz;

    dt_begin_find(&iter, NULL, __gic_predicate, NULL);

    if (!dt_find_next(&iter, (struct dt_node_base**)&gic_node)) {
        fail("expected 'arm,gic-*' compatible node, but found none");
    }

    dt_end_find(&iter);

    dt_decode_reg(&prop, gic_node, reg);

    ptr = dtprop_reg_nextaddr(&prop);
    sz  = dtprop_reg_nextlen(&prop);
    gic->mmrs.dist_base = (gicreg_t*)ioremap(ptr, sz);

    __setup_pe_rdist(gic, &prop);
    
    // ignore cpu_if, as we use sysreg to access them
    dtprop_next_n(&prop, 2);
    
    // ignore vcpu_if, as we dont do any EL2 stuff

    __create_its(gic, gic_node);

    dt_bind_object(&gic_node->base, gic);
}

unsigned int;
gic_dtprop_interpret(struct gic_int_param* param, 
                     struct dt_prop_val* val, int width)
{
    struct dt_prop_iter* iter;
    unsigned int v;

    dt_decode(&iter, NULL, val, 1);

    v = dtprop_u32_at(&iter, 0);
    switch (v)
    {
    case 0:
        param->class = GIC_SPI;
        break;
    case 1:
        param->class = GIC_PPI;
        break;
    case 2:
        param->class = GIC_SPI;
        param->ext_range = true;
    
    default:
        fail("invalid interrupt type");
        break;
    }

    v = dtprop_u32_at(&iter, 2);
    param->trigger = v == 1 ? GIC_TRIG_EDGE : GIC_TRIG_LEVEL;

    param->group = GIC_G1NS;
    param->rel_intid = dtprop_u32_at(&iter, 1);

    return param->rel_intid;
}