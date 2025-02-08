/**
 * Arm Generic Interrupt Controller v3
 */

#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/mmio.h>
#include <klibc/string.h>

#include "v3.h"

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

static inline int 
__spi_set_group(struct gic_int_class* class, 
                 intid_t intid, enum gic_intgrp grp)
{
    struct v3_distr* distr = v3distr_class(class);
    if (grp == GIC_INT_GROUP0) {
        __gic_bitmap_setf(distr->gicd->igrpmodr, intid, 0);
        __gic_bitmap_setf(distr->gicd->igroupr, intid, 0);
    }
    else if (grp == GIC_INT_GROUP1_S) {
        __gic_bitmap_setf(distr->gicd->igrpmodr, intid, 1);
        __gic_bitmap_setf(distr->gicd->igroupr, intid, 0);
    }
    else if (grp == GIC_INT_GROUP1_NS) {
        __gic_bitmap_setf(distr->gicd->igrpmodr, intid, 0);
        __gic_bitmap_setf(distr->gicd->igroupr, intid, 1);
    }
    else {
        return EINVAL;
    }
    
    return 0;
}

static int 
__spi_set_enabled(struct gic_int_class* class, intid_t intid, bool en)
{
    struct v3_distr* distr = v3distr_class(class);
    
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
    struct v3_distr* distr = v3distr_class(class);
    __gic_bitmap_setv(distr->gicd->ipriorityr, intid, prio, 8);

    return 0;
}

static int 
__spi_set_nmi(struct gic_int_class* class, intid_t intid, bool nmi)
{
    struct v3_distr* distr = v3distr_class(class);
    __gic_bitmap_setf(distr->gicd->isenabler, intid, nmi);

    return 0;
}

static int 
__spi_set_route(struct gic_int_class* class, 
               intid_t intid, struct gic_pe* target)
{
    int aff_val = target->affinity;
    int aff3 = aff_val >> 24;
    int aff210 = aff_val & ((1 << 24) - 1);

    struct v3_distr* distr = v3distr_class(class);
    distr->gicd->irouter[intid - 32] = ((u64_t)aff3 << 32) | aff210;

    return 0;
}

static int 
__spi_set_trigger(struct gic_int_class* class, 
                 intid_t intid, enum gic_trigger trig)
{
    int trig_val = trig == GIC_TRIG_EDGE ? 0b10 : 0b00;

    struct v3_distr* distr = v3distr_class(class);
    __gic_bitmap_setv(distr->gicd->icfgr, intid, trig_val, 2);

    return 0;
}


static int 
__spi_retrieve(struct gic_int_class* class, 
              struct gic_interrupt* intr, intid_t intid)
{
    int val;
    struct v3_distr* distr = v3distr_class(class);
    union v3_gicd_map* gicd = distr->gicd;
    
    intr->enabled = __gic_bitmap_getf(gicd->isenabler, intid);
    intr->nmi = __gic_bitmap_getf(gicd->inmir, intid);

    intr->priority = __gic_bitmap_getv(gicd->ipriorityr, intid, 8);

    u64_t affr = gicd->irouter[intid - 32];
    val = (((affr >> 32) & 0xff) << 24) | (affr & ((1 << 24) - 1));
    gic_intr_set_raw_affval(intr, val);

    val = __gic_bitmap_getv(gicd->icfgr, intid, 2);
    intr->trig = val == 0b10 ? GIC_TRIG_EDGE : GIC_TRIG_LEVEL;

    val = __gic_bitmap_getf(gicd->igrpmodr, intid);
    val = (val << 1) | __gic_bitmap_getf(gicd->igroupr, intid);
    if (val == 0b00) {
        intr->grp = GIC_INT_GROUP0;
    }
    else if (val == 0b01) {
        intr->grp = GIC_INT_GROUP1_NS;
    }
    else if (val == 0b10) {
        intr->grp = GIC_INT_GROUP1_S;
    }
    else {
        return EINVAL;
    }

    return 0;
}

static int 
__spi_install(struct gic_int_class* class, struct gic_interrupt* intr)
{
    __spi_set_group(class,   intr->intid, intr->grp);
    __spi_set_nmi(class,     intr->intid, intr->nmi);
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

static inline void
__lpi_write_invlpir(struct v3_pe* pe, intid_t intid)
{
    // ensure coherent for configuration write
    asm ("dsb");

    // flush the redistributor cache (only directLPI).
    if (pe->direct_lpi) {
        pe->gicr->invlpir = intid;
    }

    // ITS cache is flushed in upstream caller.
    return;
}

static int 
__lpi_set_enabled(struct gic_int_class* class, intid_t intid, bool en)
{
    struct v3_pe* pe = v3pe_class(class);
    u32_t val;
    
    assert(pe->lpi_tab);
    
    val = pe->lpi_tab[intid - 8192];
    pe->lpi_tab[intid - 8192] = (val & ~1) | (en & 1);

    __lpi_write_invlpir(pe, intid);
}

static int 
__lpi_set_prority(struct gic_int_class* class, intid_t intid, int prio)
{
    struct v3_pe* pe = v3pe_class(class);
    
    assert(pe->lpi_tab);

    pe->lpi_tab[intid - 8192] = prio & 0xfc;

    __lpi_write_invlpir(pe, intid);
}

static int 
__lpi_retrieve(struct gic_int_class* class, 
               struct gic_interrupt* intr, intid_t intid)
{
    struct v3_pe* pe = v3pe_class(class);
    u32_t val;
    
    assert(pe->lpi_tab);

    val = pe->lpi_tab[intid - 8192];

    intr->enabled = !!(val & 1);
    intr->priority = val & 0xfc;
    intr->trig = GIC_TRIG_EDGE;
    intr->grp  = GIC_INT_GROUP1_NS;
    intr->affinity.pe = pe;
    intr->nmi = false;

    return 0;
}

static inline int 
__lpi_install(struct gic_int_class* class, struct gic_interrupt* intr)
{
    struct v3_pe* pe = v3pe_class(class);
    struct irq_object* irq = intr->irq;

    __lpi_set_prority(class, intr->intid, intr->priority);
    
    if (pe->direct_lpi) {
        irq->msi->message = intr->intid;
        irq->msi->wr_addr = __ptr(&pe->gicr->setlpir);
    }
    
    // for ITS backed LPI, it will installed separately, so we are done.
    return 0;
}

static bool
__pe_ack_interrupt(struct gic_pe* pe)
{
    intid_t id;
    
    id = read_sysreg(ICC_IAR1_EL1);
    switch (id)
    {
    case INTID_IAR1_NMI:
        id = read_sysreg(ICC_NMIAR1_EL1);
        break;
    
    case INTID_NOTHING:
        return false;

    case INTID_ACKED_NS:
    case INTID_ACKED_S:
        fail("impossible intid");
        break;
    }

    pe->active_id = id;
    return true;
}

static int
__pe_notify_eoi(struct gic_pe* pe)
{
    if (!pe->has_active_int) {
        return 0;
    }

    assert(pe->active_int.grp == GIC_INT_GROUP1_NS);
    
    set_sysreg(ICC_EOIR1_EL1, pe->active_id);
    set_sysreg(ICC_DIR_EL1, pe->active_id);
    return 0;
}

static int 
__pe_set_priority_mask(struct gic_pe* pe, int mask)
{
    set_sysreg(ICC_PMR_EL1, mask & 0xff);
    return 0;
}

static struct gic_int_class_ops spi_ops = {
    .set_enabled = __spi_set_enabled,
    .set_nmi = __spi_set_nmi,
    .set_prority = __spi_set_prority,
    .set_route = __spi_set_route,
    .set_trigger = __spi_set_trigger,
    .install = __spi_install,
    .retrieve = __spi_retrieve,
    .delete = __spi_delete
};

static struct gic_int_class_ops lpi_ops = {
    .set_enabled = __lpi_set_enabled,
    .set_nmi = __fallback_set_nmi,
    .set_prority = __lpi_set_prority,
    .set_route = __fallback_set_route,
    .set_trigger = __fallback_set_trigger,
    .install = __lpi_install,
    .retrieve = __lpi_retrieve,
    .delete = __fallback_delete
};

static struct gic_pe_ops pe_ops = {
    .ack_int = __pe_ack_interrupt,
    .notify_eoi = __pe_notify_eoi,
    .set_priority_mask = __pe_set_priority_mask
};

static void
v3_enable_interrupt(struct gic* gic)
{
    struct v3_pe* v3pe;
    struct v3_distr* distr;

    set_sysreg(ICC_IGRPEN1_EL1, ICC_IGRPEN_ENABLE);

    for (int i = 0; i < gic->nr_cpus; i++)
    {
        assert(gic->cores[i]);

        v3pe = (struct v3_pe*)gic->cores[i]->impl;
        v3pe->gicr->ctlr |= GICR_CTLR_EnLPI;
    }

    distr = (struct v3_distr*)gic->impl;
    distr->gicd->ctlr |= GICD_CTLR_G1NSEN;
}

static void
v3_config_redistr(struct v3_pe* v3_pe)
{
    struct gic_pe* pe;
    struct leaflet* page;
    u64_t val;
    int nr_pages, nr_lpis;

    pe = v3_pe->pe;

    // Configure LPI Tables

    if (!gic_valid_int_class(&pe->lpi)) {
        return;
    }

    // We only use at most 8192 LPIs
    nr_lpis = 8192;
    nr_pages = page_count(8192, PAGE_SIZE);

    
    /*
        We map all the LPI tables as device memory (nGnRnE),
        and Non-shareable. However, these combination implies 
        Outer-Shareable (R_PYFVQ), which ensure visibility across
        all PEs and clusters
    */

    // 8192 LPIs, two 4K pages
    page = alloc_leaflet_pinned(to_napot_order(nr_pages));
    leaflet_wipe(page);
    val = leaflet_addr(page) | (ilog2(nr_lpis) + 1);
    v3_pe->gicr->propbaser = val;

    // 8192 LPIs, 1024K sized bmp, consider 64K alignment
    nr_pages = page_count(PAGE_64K, PAGE_SIZE);
    page = alloc_leaflet_pinned(to_napot_order(nr_pages));
    leaflet_wipe(page);
    v3_pe->lpi_tab_ptr = vmap(page, KERNEL_DATA);

    /*
        Alignment correction, 
        given a 64K-unaligned base address of 64K leaflet
        any upalignment to 64K boundary still falls within the
        leaflet, thus always leaving at least one free base-page
    */
    val = leaflet_addr(page);
    if (val & (PAGE_64K - 1)) {
        val = napot_upaligned(val, PAGE_64K);
    }

    val = val | GICR_PENDBASER_PTZ | (ilog2(nr_lpis) + 1);
    v3_pe->gicr->pendbaser = val;
}

static void
v3_config_icc(struct v3_pe* pe)
{
    sysreg_flagging(ICC_CTLR_EL1, 0, ICC_CTRL_EOImode);
    sysreg_flagging(ICC_CTLR_EL1, ICC_CTRL_CBPR, 0);
    sysreg_flagging(ICC_CTLR_EL1, ICC_CTRL_PMHE, 0);
    
    set_sysreg(ICC_PMR_EL1, 0xff);  // all 256 priority values
    set_sysreg(ICC_BPR0_EL1, 7);    // no preemption
}

static void
v3_config_distr(struct gic* gic, struct v3_distr* distr)
{
    u32_t ctlr;
    union v3_gicd_map* gicd;
    unsigned int intid_max, lpi_max;

    gicd = distr->gicd;

    ctlr = GICD_CTLR_DS | GICD_CTLR_ARE_NS;
    gicd->ctlr = ctlr;

    intid_max = BITS_GET(gicd->typer, GICD_TYPER_nSPI);
    if (intid_max) {
        intid_max = 32 * (intid_max + 1) - 1;
        gic_init_int_class(&gic->spi, GIC_INT_SPI, 32, intid_max);
        gic->spi.ops = &spi_ops;
    }

    intid_max = BITS_GET(gicd->typer, GICD_TYPER_nESPI);
    if ((gicd->typer & GICD_TYPER_ESPI)) {
        intid_max = 32 * (intid_max + 1) + 4095;
        gic_init_int_class(&gic->spi_e, GIC_INT_SPI_EXT, 4096, intid_max);
    }
}

static void
v3_config_pe(struct gic* gic, struct gic_pe* pe)
{
    struct v3_pe* v3_pe;
    unsigned int affval, ppi_num;
    unsigned int intid_max, lpi_max;

    union v3_rdbase_map *gicr;
    union v3_gicd_map* gicd;

    v3_pe = v3pe(pe);
    gicd  = v3distr(gic)->gicd;
    gicr  = v3_pe->gicr;
    
    affval  = BITS_GET(gicr->typer, GICR_TYPER_AffVal);
    ppi_num = BITS_GET(gicr->typer, GICR_TYPER_PPInum);

    pe->affinity = affval;
    v3_pe->direct_lpi = !!(gicr->typer & GICR_TYPER_DirectLPI);

    gic_init_int_class(&pe->sgi, GIC_INT_SGI, 0, 15);
    gic_init_int_class(&pe->ppi, GIC_INT_PPI, 16, 31);

    switch (ppi_num)
    {
    case 0b00001:
        gic_init_int_class(&pe->ppi_e, GIC_INT_PPI_EXT, 1056, 1087);
        break;
        
    case 0b00010:
        gic_init_int_class(&pe->ppi_e, GIC_INT_PPI_EXT, 1056, 1119);
        break;
    }

    intid_max = BITS_GET(gicd->typer, GICD_TYPER_IDbits);
    intid_max = 1 << (intid_max + 1);

    if (intid_max > 8192) {
        // NOTE the minimum support of 8192 LPIs is far enough.

        // lpi_max = BITS_GET(gicd->typer, GICD_TYPER_nLPI);
        // if (lpi_max) {
        //     lpi_max = ((1 << (lpi_max + 1)) - 1) + 8192;
        // } else {
        //     lpi_max = intid_max - 1;
        // }

        gic_init_int_class(&pe->lpi, GIC_INT_LPI, 8192, 8192 * 2);
        pe->lpi.ops = &lpi_ops;
    }

    v3_config_redistr(pe);
    v3_config_icc(pe);
}

/*
 *      ITS specifics
 */

static ptr_t
__its_alloc_itt(struct v3_its* its)
{
    struct leaflet* leaflet;
    struct v3_itt_alloc* ita;

    ita = &its->itt_alloc;

restart:
    if (unlikely(ita->page)) {
        leaflet = alloc_leaflet_pinned(0);
        leaflet_wipe(leaflet);

        ita->page = leaflet_addr(leaflet);
        memset(ita->free_map, 0, sizeof(ita->free_map));
    }

    for (int i = 0; i < sizeof(ita->free_map); i++)
    {
        if (ita->free_map[i] == 0xff)
            continue;

        for (int j = 0; j < 8; j++) {
            if (ita->free_map[i] & (1 << j)) {
                continue;
            }

            ita->free_map[i] |= 1 << j;
            return ita->page + (i * 8 + j) * 256;
        }
    }
    
    ita->page = NULL;
    goto restart;
}

#define irq_devid(irq)  ((irq)->msi->sideband)
#define irq_evtid(irq)  ((irq)->msi->message)

static void
__its_cmd_inv(struct v3_its_cmd* cmd, irq_t irq)
{
    *cmd = (struct v3_its_cmd) {
        .dw0 = (u64_t)irq_devid(irq) << 32 | 0xc,
        .dw1 = irq_evtid(irq)
    };
}

static void
__its_cmd_mapc(struct v3_its_cmd* cmd, struct v3_pe* v3_pe)
{
    *cmd = (struct v3_its_cmd) {
        .dw0 = 0x9,
        .dw2 = (1UL << 63) | v3_pe->pe->index | v3_pe->rd_base
    };
}

static void
__its_cmd_mapd(struct v3_its_cmd* cmd, struct v3_its* its, irq_t irq)
{
    *cmd = (struct v3_its_cmd) {
        .dw0 = (u64_t)irq_devid(irq) << 32 | 0x8,
        .dw1 = ilog2(8) - 1,
        .dw2 = (1UL << 63) | __its_alloc_itt(its)
    };
}

static void
__its_cmd_mapti(struct v3_its_cmd* cmd, irq_t irq, int pe_index)
{
    *cmd = (struct v3_its_cmd) {
        .dw0 = (u64_t)irq_devid(irq) << 32 | 0xa,
        .dw1 = (u64_t)irq->vector << 32 | irq_evtid(irq),
        .dw2 = pe_index
    };
}

static void
__its_issue_cmd(struct v3_its* its, struct v3_its_cmd* cmd)
{
    ptr_t wr, rd, base;

    wr = its->gits->cwriter >> 5;
    do {
        rd = its->gits->creadr >> 5;
    } while (wr > rd && wr - rd == 1);

    its->cmdq[wr] = *cmd;
    wr = (wr + 1) % its->cmdq_size;

    its->gits->cwriter = wr << 5;
}

static int
__its_domain_install_irq(struct irq_domain *itsd, irq_t irq)
{
    int err;
    struct v3_its* its;
    struct v3_its_cmd cmd;
    struct gic_pe* pe;
    struct gic_interrupt gici;

    its = irq_domain_obj(itsd, struct v3_its);

    if (irq->type != IRQ_MESSAGE) {
        return EINVAL;
    }

    pe = its->gic->cores[0];
    err = gic_assign_intid(&pe->lpi, irq);

    if (err) 
        return err;

    gici.intid = irq->vector;
    gici.irq = irq;
    gici.priority = 255;

    __lpi_install(&pe->lpi, &gici);

    __its_cmd_mapd(&cmd, its, irq);
    __its_issue_cmd(its, &cmd);

    __its_cmd_mapti(&cmd, irq, 0);
    __its_issue_cmd(its, &cmd);

    __its_cmd_inv(&cmd, irq);
    __its_issue_cmd(its, &cmd);

    __lpi_set_enabled(&pe->lpi, irq->vector, true);

    irq->msi->wr_addr = __ptr(&its->gtrn->translater);

    return 0;
}

static int
__its_domain_remove_irq(struct irq_domain *itsd, irq_t irq)
{
    // TODO
    return 0;
}

static struct irq_domain_ops its_ops = {
    .install_irq = __its_domain_install_irq,
    .remove_irq = __its_domain_remove_irq
};

static u64_t
__setup_basern_memory(struct v3_its* its, size_t ent_sz, size_t nr_ents)
{
    u64_t val, *ind_pages;
    int nr_pages, pgsize;
    struct leaflet* page, *sub_page;

    nr_pages = page_count(ent_sz * nr_ents, PAGE_SIZE);
    page = alloc_leaflet_pinned(0);
    leaflet_wipe(page);

    val = GITS_BASER_VALID;
    switch (PAGE_SIZE) {
        case 4096 * 4:
            pgsize = 0b01;
            break;
        case 4096 * 16:
            pgsize = 0b10;
            break;
    }
    BITS_SET(val, GITS_BASERn_PGSZ, pgsize);

    if (nr_pages == 1) {
        val |= leaflet_addr(page);
        return val;
    }

    ind_pages = (u64_t*)vmap(page, KERNEL_DATA);
    for (int i = 0; i < nr_pages; i++)
    {
        sub_page = alloc_leaflet_pinned(0);
        leaflet_wipe(sub_page);
        ind_pages[i] = val | leaflet_addr(sub_page);
    }

    vunmap(__ptr(ind_pages), page);
    val |= GITS_BASER_Ind | leaflet_addr(page);
    
    return val;
}

static struct irq_domain*
v3_init_its_domain(struct gic* gic, 
                   struct device* its_dev, struct v3_its* its)
{
    u64_t val;
    struct leaflet* page;
    struct irq_domain* domain;
    union v3_its_regs* gits;
    struct v3_its_cmd cmd;

    domain = irq_create_domain(its_dev, &its_ops);
    gits = its->gits;

    page = alloc_leaflet_pinned(0);
    leaflet_wipe(page);

    val = GITS_BASER_VALID | leaflet_addr(page);
    gits->cbaser = val;
    its->cmdq = vmap(page, KERNEL_DATA);
    its->cmdq_size = PAGE_SIZE / sizeof(struct v3_its_cmd);

    // Setup BASER<n>

    int type;
    size_t ent_sz, nr_ents;
    for (int i = 0; i < 8; i++)
    {
        val = gits->basern[i];
        
        type = BITS_GET(val, GITS_BASERn_TYPE);
        if (!type || type == 0b010) {
            continue;
        }

        if (type == 0b001) {
            nr_ents = BITS_GET(gits->typer, GITS_TYPER_Devbits);
            nr_ents = 1 << (nr_ents + 1);
        }
        else if (type) {
            nr_ents = gic->nr_cpus;
        }

        ent_sz = BITS_GET(val, GITS_BASERn_EntSz);
        gits->basern[i] = __setup_basern_memory(its, ent_sz, nr_ents);
    }

    // enable ITS command engine

    gits->creadr = 0;
    gits->cwriter = 0;
    gits->ctlr |= GITS_CTLR_EN;

    // map each core to a collection

    for (int i = 0; i < gic->nr_cpus; i++)
    {
        __its_cmd_mapc(&cmd, v3pe(gic->cores[i]));
        __its_issue_cmd(its, &cmd);
    }
    
    its->gic = gic;
    irq_set_domain_object(domain, its);

    return domain;
}

static inline ptr_t
v3_map_reg(struct dtpropi* reg_dtpi, struct dtn* dtn, 
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

static struct v3_distr*
v3_map_distributor(struct gic* gic, 
                   struct dtpropi* reg_dtpi, struct dtn* dtn)
{
    struct v3_distr* distr;

    distr = valloc(sizeof(*distr));

    distr->dist_ptr = v3_map_reg(reg_dtpi, dtn, NULL, NULL);

    distr->gic = gic;
    gic->impl = distr;

    return distr;
}

static void
v3_map_redistributors(struct gic* gic, 
                      struct dtpropi* reg_dtpi, struct dtn* dtn)
{
    struct dtp_val* val;
    struct v3_pe* v3_pe;
    int red_stride, nr_red_regions, nr_reds;
    size_t region_sz, step;
    ptr_t region_base, region_base_phy;

    val = dt_getprop(dtn, "#redistributor-regions");
    nr_red_regions = val ? val->ref->u32_val : 1;

    val = dt_getprop(dtn, "redistributor-stride");
    red_stride = val ? val->ref->u32_val : 0;

    /*
        We assume only a max 16 cores in all scenarios,
        no doubt a bad assumption, but the kernel is uniprocessor,
        just show some respects to the gic.
    */
    struct gic_pe *pe, *pes[16];
    for (int i = 0; i < nr_red_regions; i++)
    {
        step = 0;
        region_base = v3_map_reg(reg_dtpi, dtn, &region_base_phy, &region_sz);

        do {
            v3_pe = valloc(sizeof(*v3_pe));
            v3_pe->rd_base = region_base_phy + step;
            v3_pe->red_ptr = region_base + step;
            v3_pe->sgi_ptr = region_base + step + GIC_FRAME_SIZE;

            pe = gic_create_pe_context(gic, v3_pe);
            pe->index = nr_reds;
            
            v3_pe->pe = pe;
            pes[nr_reds] = pe;

            nr_reds++;
            step += red_stride;
        } while (red_stride && step < region_sz);
    }
    
    gic->cores = vcalloc(sizeof(struct gic_pe*), nr_reds);
    gic->nr_cpus = nr_reds;

    memcpy(gic->cores, pes, sizeof(pes));
}

static bool
__its_predicate(struct dtn_iter* iter, struct dtn_base* pos)
{
    return strneq(pos->compat.str_val, "arm,gic-v3-its", 14);
}

static struct devclass its_class = DEVCLASS(ARM, CFG, ITS);

static void
v3_init_its_device(struct gic* gic, 
                   struct dtpropi* reg_dtpi, struct dtn* dtn)
{
    struct v3_its* its;
    struct device* itsdev;
    ptr_t base_phys;

    its = valloc(sizeof(*its));
    its->base_ptr = v3_map_reg(&reg_dtpi, dtn, &base_phys, NULL);
    its->trns_phys_ptr = base_phys + GIC_FRAME_SIZE;
    its->gic = gic;

    itsdev = device_allocsys(NULL, its);
    device_set_devtree_node(itsdev, dtn);
    register_device_var(itsdev, &its_class, "gic-its");

    v3_init_its_domain(gic, itsdev, its);
}

static void
v3_probe_its_devices(struct gic* gic, struct dtn* dtn)
{
    struct dtpropi* dtpi;
    struct dtn* its_node;
    struct dtn_iter iter;

    dt_begin_find(&iter, dtn, __its_predicate, NULL);

    while (dt_find_next(&iter, (struct dtn_base**)&its_node))
    {
        dtpi_init(&dtpi, &its_node->reg);
        v3_init_its_device(gic, &dtpi, its_node);
    }
    
    dt_end_find(&iter);
}

static struct gic*
v3_init(struct device_def* def, struct dtn* dtn)
{
    struct gic* gic;
    struct device* v3dev;
    struct dtpropi* dtpi;

    v3dev = device_allocsys(NULL, NULL);
    device_set_devtree_node(v3dev, dtn);
    register_device(v3dev, &def->class, "gic");

    gic = gic_create_context(v3dev);

    dtpi_init(&dtpi, &dtn->reg);
    v3_map_distributor(gic, &dtpi, dtn);
    v3_map_redistributors(gic, &dtpi, dtn);
    
    v3_probe_its_devices(gic, dtn);

    return gic;
}

static void
gic_register(struct device_def* def)
{
    dtm_register_entry(def, "arm,gic-v3");
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

    gic = v3_init(def, node);

    v3_config_distr(gic, v3distr(gic));
    for (int i = 0; i < gic->nr_cpus; i++)
    {
        v3_config_pe(gic, gic->cores[i]);
    }
    
    v3_enable_interrupt(gic);
}


static struct device_def dev_arm_gic = {
    def_device_name("arm gic-v3"),
    def_device_class(ARM, CFG, GIC),

    def_on_register(gic_register),
    def_on_create(gic_init)
};
EXPORT_DEVICE(arm_gic, &dev_arm_gic, load_sysconf);