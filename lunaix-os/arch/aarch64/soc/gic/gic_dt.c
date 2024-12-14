#include <lunaix/types.h>
#include <lunaix/mm/mmio.h>
#include <asm/soc/gic.h>

#include <klibc/string.h>

static bool
__its_predicate(struct dt_node_iter* iter, struct dtn_base* pos)
{
    return strneq(pos->compat.str_val, "arm,gic-v3-its", 14);
}

static inline void*
__do_remap_mmio(struct dtpropx* dtpx, int row)
{
    struct dtprop_xval base, size;

    dtpx_extract_loc(&dtpx, &base, row, 0);
    dtpx_extract_loc(&dtpx, &size, row, 1);

    return ioremap(base.u64, size.u64);
}

static inline void*
__remap_mmio_dtn(struct dtn* node)
{
    struct dtprop_xval base, size;
    struct dtpropx dtpx;
    dt_proplet gic_mmio =  dtprop_reglike(&node->base);

    dtpx_compile_proplet(gic_mmio);
    dtpx_prepare_with(&dtpx, &node->reg, &gic_mmio);

    return __do_remap_mmio(&dtpx, 0);
}

static void
__create_its(struct arm_gic* gic, struct dtn* gic_node)
{
    struct dt_node* its_node;
    struct dtn_iter iter;

    dt_begin_find(&iter, gic_node, __its_predicate, NULL);

    while (dt_find_next(&iter, (struct dt_node_base**)&its_node))
    {
        gic_its_create(gic, __remap_mmio_dtn(its_node));
    }
    
    dt_end_find(&iter);
}

void
gic_create_from_dt(struct arm_gic* gic, struct dtn* node)
{
    struct dtpropx dtpx;
    struct gic_rd* rd;
    dt_proplet gic_mmio =  dtprop_reglike(&node->base);

    dtpx_compile_proplet(gic_mmio);
    dtpx_prepare_with(&dtpx, &node->reg, &gic_mmio);

    gic->mmrs.dist_base = (gicreg_t*)__do_remap_mmio(&dtpx, 0);

    for (int i = 0; i < NR_CPU; i++) {
        rd = (struct gic_rd*)__do_remap_mmio(&dtpx, i + 1);
        gic->pes[i]._rd = rd;
    }
    
    // ignore cpu_if, as we use sysreg to access them    
    // ignore vcpu_if, as we dont do any EL2 stuff

    __create_its(gic, node);
}

unsigned int;
gic_decode_specifier(struct gic_int_param* param, 
                     struct dtp_val* val, int nr_cells)
{
    struct dtpropi dtpi;
    unsigned int v;

    assert(nr_cells >= 3);
    dtpi_init(&dtpi, val);

    v = dtpi_next_u32(&dtpi);
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

    param->group = GIC_G1NS;
    param->rel_intid = dtpi_next_u32(&dtpi);
    
    v = dtpi_next_u32(&dtpi);
    param->trigger = v == 1 ? GIC_TRIG_EDGE : GIC_TRIG_LEVEL;

    // 4th cell only applicable to PPI, ignore for now

    return param->rel_intid;
}