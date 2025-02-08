/**
 * Arm Generic Interrupt Controller v2
 */

#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/mmio.h>
#include <klibc/string.h>

#include "v2.h"

static inline void
__gic_bitmap_setf(u32_t* bmp, intid_t intid, bool flag)
{
    if (flag) {
        bmp[intid / 32] |= 1 << (intid % 32);
    } else {
        bmp[intid / 32] &= ~(1 << (intid % 32));
    }
}

static inline bool
__gic_bitmap_getf(u32_t* bmp, intid_t intid)
{
    return !!(bmp[intid / 32] & (1 << (intid % 32)));
}

static inline void
__gic_bitmap_setv(u32_t* bmp, intid_t intid, int val, u32_t width_bits)
{
    u32_t slots = 32 / width_bits;
    u32_t mask = (1 << width_bits) - 1;
    u32_t _val;

    _val  = bmp[intid / slots];
    _val &= ~(mask << (intid % slots));
    bmp[intid / slots] =  (_val & mask) << (intid % slots);
}

static inline int
__gic_bitmap_getv(u32_t* bmp, intid_t intid, u32_t width_bits)
{
    u32_t slots = 32 / width_bits;
    u32_t mask = (1 << width_bits) - 1;

    return !!(bmp[intid / slots] & (mask << (intid % slots)));
}

// 4.3.4 Interrupt Group Registers, GICD_IGROUPRn
static inline int 
__spi_set_group(struct gic_int_class* class, 
                 intid_t intid, enum gic_intgrp grp)
{
    struct v2_distr* distr = v2distr_class(class);
    if (grp == GIC_INT_GROUP1_NS) {
        __gic_bitmap_setf(distr->gicd->igrpmodr, intid, 0);
        __gic_bitmap_setf(distr->gicd->igroupr, intid, 1);
    } else {
        return EINVAL;
    }
    
    return 0;
}

static int 
__spi_set_enabled(struct gic_int_class* class, intid_t intid, bool en)
{
    struct v2_distr* distr = v2distr_class(class);
    
    if (en) {
        __gic_bitmap_setf(distr->gicd->isenabler, intid, true);
    } else {
        __gic_bitmap_setf(distr->gicd->icenabler, intid, false);
    }

    return 0;
}

static int 
__spi_set_prority(struct gic_int_class* class, intid_t intid, int prio)
{
    struct v2_distr* distr = v2distr_class(class);
    __gic_bitmap_setv(distr->gicd->ipriorityr, intid, prio, 8);

    return 0;
}

static int 
__spi_set_route(struct gic_int_class* class, 
               intid_t intid, struct gic_pe* target)
{
    int aff_val = target->affinity;
    int aff3 = aff_val >> 24;
    int aff210 = aff_val & ((1 << 24) - 1);

    struct v2_distr* distr = v2distr_class(class);
    distr->gicd->irouter[intid - 32] = ((u64_t)aff3 << 32) | aff210;

    return 0;
}

// Table 4-18 GICD_ICFGR bit assignments
static int 
__spi_set_trigger(struct gic_int_class* class, 
                 intid_t intid, enum gic_trigger trig)
{
    int trig_val = trig == GIC_TRIG_EDGE ? 0b10 : 0b00;

    struct v2_distr* distr = v2distr_class(class);
    __gic_bitmap_setv(distr->gicd->icfgr, intid, trig_val, 2);

    return 0;
}


static int 
__spi_retrieve(struct gic_int_class* class, 
              struct gic_interrupt* intr, intid_t intid)
{
    int val;
    struct v2_distr* distr = v2distr_class(class);
    union v2_gicd_map* gicd = distr->gicd;
    
    intr->enabled = __gic_bitmap_getf(gicd->isenabler, intid);
    intr->nmi = false;
    intr->priority = __gic_bitmap_getv(gicd->ipriorityr, intid, 8);

    val = __gic_bitmap_getv(gicd->icfgr, intid, 2);
    intr->trig = val == 0b10 ? GIC_TRIG_EDGE : GIC_TRIG_LEVEL;

    val = __gic_bitmap_getf(gicd->igrpmodr, intid);
    val = (val << 1) | __gic_bitmap_getf(gicd->igroupr, intid);
    if (val == 0b01) {
        intr->grp = GIC_INT_GROUP1_NS;
    } else {
        return EINVAL;
    }

    return 0;
}

static int 
__spi_install(struct gic_int_class* class, struct gic_interrupt* intr)
{
    __spi_set_group(class,   intr->intid, intr->grp);
    __spi_set_prority(class, intr->intid, intr->priority);
    __spi_set_route(class,   intr->intid, intr->affinity.pe);
    __spi_set_trigger(class, intr->intid, intr->trig);

    return 0;
}

static int 
__spi_delete(struct gic_int_class* class, intid_t intid)
{
    __spi_set_enabled(class, intid, false);
}

// 4.4.4 Interrupt Acknowledge Register, GICC_IAR
static bool
__pe_ack_interrupt(struct gic_pe* pe)
{
    return true;
}

static int
__pe_notify_eoi(struct gic_pe* pe)
{
    return 0;
}

static int 
__pe_set_priority_mask(struct gic_pe* pe, int mask)
{
    return 0;
}

static struct gic_int_class_ops spi_ops = {
    .set_enabled = __spi_set_enabled,
    .set_nmi = NULL,
    .set_prority = __spi_set_prority,
    .set_route = __spi_set_route,
    .set_trigger = __spi_set_trigger,
    .install = __spi_install,
    .retrieve = __spi_retrieve,
    .delete = __spi_delete
};

static struct gic_pe_ops pe_ops = {
    .ack_int = __pe_ack_interrupt,
    .notify_eoi = __pe_notify_eoi,
    .set_priority_mask = __pe_set_priority_mask
};

// 3.2.2 Interrupt controls in the GIC
static void
v2_enable_interrupt(struct gic* gic)
{
    struct v2_pe* v2pe;
    struct v2_distr* distr;
    distr = (struct v2_distr*)gic->impl;
    distr->gicd->ctlr |= GICD_CTLR_G1NSEN;
    // GICD_ISENABLERn
    // distr->gicd->isenabler |= 0xffffffff;
}

static void
v2_config_distr(struct gic* gic, struct v2_distr* distr)
{
    u32_t ctlr;
    union v2_gicd_map* gicd;
    unsigned int intid_max, ;

    gicd = distr->gicd;
    gicd->ctlr = 0;

    intid_max = BITS_GET(gicd->typer, GICD_TYPER_nSPI);
    if (intid_max) {
        intid_max = 32 * (intid_max + 1) - 1;
        gic_init_int_class(&gic->spi, GIC_INT_SPI, 32, intid_max);
        gic->spi.ops = &spi_ops;
    }
}

static void
v2_config_pe(struct gic* gic, struct gic_pe* pe)
{
    struct v2_pe* v2_pe;

    union v2_gicd_map* gicd;

    v2_pe = v2pe(pe);
    gicd  = v2distr(gic)->gicd;

    gic_init_int_class(&pe->sgi, GIC_INT_SGI, 0, 15);
    gic_init_int_class(&pe->ppi, GIC_INT_PPI, 16, 31);

}

#define irq_devid(irq)  ((irq)->msi->sideband)
#define irq_evtid(irq)  ((irq)->msi->message)

static inline ptr_t
v2_map_reg(struct dtpropi* reg_dtpi, struct dtn* dtn, 
                 ptr_t* phy_base_out, size_t* size_out)
{
    ptr_t base, size;

    assert(dtpi_has_next(reg_dtpi));

    base = dtpi_next_integer(reg_dtpi, dtn->base.addr_c);
    size = dtpi_next_integer(reg_dtpi, dtn->base.sz_c);

    if (size_out)
        *size_out = size;

    if (phy_base_out)
        *phy_base_out = base;

    return ioremap(base, size);
}

static struct v2_distr*
v2_map_distributor(struct gic* gic, 
                   struct dtpropi* reg_dtpi, struct dtn* dtn)
{
    struct v2_distr* distr;

    distr = valloc(sizeof(*distr));

    distr->dist_ptr = v2_map_reg(reg_dtpi, dtn, NULL, NULL);

    distr->gic = gic;
    gic->impl = distr;

    return distr;
}

static void
v2_map_redistributors(struct gic* gic, 
                      struct dtpropi* reg_dtpi, struct dtn* dtn)
{
    struct dtp_val* val;
    struct v2_pe* v2_pe;
    int red_stride, nr_red_regions, nr_reds;
    size_t region_sz, step;
    ptr_t region_base, region_base_phy;

    val = dt_getprop(dtn, "#redistributor-regions");
    nr_red_regions = val ? val->ref->u32_val : 1;

    val = dt_getprop(dtn, "redistributor-stride");
    red_stride = val ? val->ref->u32_val : 0;

    struct gic_pe *pe, *pes[16];
    for (int i = 0; i < nr_red_regions; i++)
    {
        step = 0;
        region_base = v2_map_reg(reg_dtpi, dtn, &region_base_phy, &region_sz);

        do {
            v2_pe = valloc(sizeof(*v2_pe));
            v2_pe->rd_base = region_base_phy + step;
            v2_pe->red_ptr = region_base + step;
            v2_pe->sgi_ptr = region_base + step + GIC_FRAME_SIZE;

            pe = gic_create_pe_context(gic, v2_pe);
            pe->index = nr_reds;
            
            v2_pe->pe = pe;
            pes[nr_reds] = pe;

            nr_reds++;
            step += red_stride;
        } while (red_stride && step < region_sz);
    }
    
    gic->cores = vcalloc(sizeof(struct gic_pe*), nr_reds);
    gic->nr_cpus = nr_reds;

    memcpy(gic->cores, pes, sizeof(pes));
}

static struct gic*
v2_init(struct device_def* def, struct dtn* dtn)
{
    struct gic* gic;
    struct device* v2dev;
    struct dtpropi* dtpi;

    v2dev = device_allocsys(NULL, NULL);
    device_set_devtree_node(v2dev, dtn);
    register_device(v2dev, &def->class, "gic");

    gic = gic_create_context(v2dev);

    dtpi_init(&dtpi, &dtn->reg);
    v2_map_distributor(gic, &dtpi, dtn);

    return gic;
}

static void
gic_register(struct device_def* def)
{
    dtm_register_entry(def, "arm,gic-v2");
}

static void
gic_init(struct device_def* def, morph_t* mobj)
{
    struct gic* gic;
    struct dtn* node;

    node = changeling_try_reveal(mobj, dt_morpher);
    if (!node) {
        return;
    }

    gic = v2_init(def, node);

    v2_config_distr(gic, v2distr(gic));
    for (int i = 0; i < gic->nr_cpus; i++)
    {
        v2_config_pe(gic, gic->cores[i]);
    }
    
    v2_enable_interrupt(gic);
}


static struct device_def dev_arm_gic = {
    def_device_name("arm gic-v2"),
    def_device_class(ARM, CFG, GIC),

    def_on_register(gic_register),
    def_on_create(gic_init)
};
EXPORT_DEVICE(arm_gic, &dev_arm_gic, load_sysconf);