#include <lunaix/types.h>
#include <lunaix/device.h>
#include <lunaix/spike.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/syslog.h>

#include <hal/devtree.h>

#include <asm/aa64_isrm.h>
#include <asm/soc/gic.h>

static struct arm_gic gic;

LOG_MODULE("gic")

DEFINE_BMP_INIT_OP(gic_bmp, valloc);

DEFINE_BMP_QUERY_OP(gic_bmp);

DEFINE_BMP_SET_OP(gic_bmp);

DEFINE_BMP_ALLOCFROM_OP(gic_bmp);

/* ++++++ GIC dirver ++++++ */

/* ****** Interrupt Management ****** */

static void
__config_interrupt(struct arm_gic* gic, struct gic_distributor* dist, 
                    struct gic_interrupt* ent)
{
    unsigned int intid_rel;
    unsigned long trig_index;

    intid_rel = ent->intid - ent->domain->base;

    if (ent->config.class == GIC_LPI) {
        lpi_entry_t entry = 0;

        entry |= LPI_EN;
        
        gic->lpi_tables.prop_table[intid_rel] = entry;

        // clear any pending when we (re-)configuring
        bitmap_set(gic_bmp, &gic->lpi_tables.pendings, intid_rel, false);

        return;
    }


    bitmap_set(gic_bmp, &dist->group, intid_rel, 0);
    bitmap_set(gic_bmp, &dist->grpmod, intid_rel, 1);

    trig_index = intid_rel * 2;
    bitmap_set(gic_bmp, &dist->icfg, trig_index, 0);
    if (ent->config.trigger == GIC_TRIG_EDGE) {
        bitmap_set(gic_bmp, &dist->icfg, trig_index + 1, 1);
    } else {
        bitmap_set(gic_bmp, &dist->icfg, trig_index + 1, 0);
    }

    if (gic->nmi_ready) {
        bitmap_set(gic_bmp, &dist->nmi, intid_rel, ent->config.as_nmi);
    }

    bitmap_set(gic_bmp, &dist->enable, intid_rel, true);
}

static void
__undone_interrupt(struct arm_gic* gic, struct gic_distributor* dist, 
                    struct gic_interrupt* ent)
{
    unsigned int intid_rel;

    intid_rel = ent->intid - ent->domain->base;
    
    if (ent->config.class == GIC_LPI) {
        gic->lpi_tables.prop_table[intid_rel] = 0;

        // clear any pending when we (re-)configuring
        bitmap_set(gic_bmp, &gic->lpi_tables.pendings, intid_rel, false);

        return;
    }

    bitmap_set(gic_bmp, &dist->disable, intid_rel, true);
    
    if (gic->nmi_ready) {
        bitmap_set(gic_bmp, &dist->nmi, intid_rel, false);
    }
}

static inline struct gic_interrupt*
__get_interrupt(struct gic_idomain* domain, unsigned int rel_intid)
{
    struct gic_interrupt *pos, *n;
    unsigned int intid;

    intid = rel_intid + domain->base;
    hashtable_hash_foreach(domain->recs, intid, pos, n, node)
    {
        if (pos->intid == intid) {
            return pos;
        }
    }

    return NULL;
}

static struct gic_interrupt*
__find_interrupt_record(unsigned int intid)
{
    struct gic_idomain* domain;

    domain = __deduce_domain(intid);
    
    if (!domain) {
        return NULL;
    }

    return __get_interrupt(domain, intid - domain->base);
}

static inline struct gic_interrupt*
__register_interrupt(struct gic_idomain* domain, 
                            unsigned int intid, struct gic_int_param* param)
{
    struct gic_interrupt* interrupt;

    interrupt = valloc(sizeof(*interrupt));
    interrupt->config = (struct gic_intcfg) {
        .class   = param->class,
        .trigger = param->trigger,
        .group   = param->group,
        .as_nmi  = param->as_nmi
    };

    interrupt->intid = intid;
    interrupt->domain = domain;

    hashtable_hash_in(domain->recs, &interrupt->node, intid);

    return interrupt;
}


/* ****** Interrupt Domain Management ****** */

static struct gic_idomain*
__deduce_domain(unsigned int intid)
{
    if (intid <= INITID_SGI_END) {
        return gic.pes[0].idomain.local_ints;
    }

    if (intid <= INITID_PPI_END) {
        return gic.pes[0].idomain.local_ints;
    }

    if (intid <= INITID_SPI_END) {
        return gic.idomain.spi;
    }

    if (INITID_ePPI_BASE <= intid && intid <= INITID_ePPI_END) {
        return gic.pes[0].idomain.eppi;
    }

    if (INITID_eSPI_BASE <= intid && intid <= INITID_eSPI_END) {
        return gic.idomain.espi;
    }

    if (intid >= INITID_LPI_BASE) {
        return gic.idomain.lpi;
    }

    return NULL;
}

static struct gic_idomain*
__idomain(int nr_ints, unsigned int base, bool extended)
{
    struct gic_idomain* rec;

    rec = valloc(sizeof(*rec));
    
    bitmap_init(gic_bmp, &rec->ivmap, nr_ints);
    hashtable_init(rec->recs);

    rec->base = base;
    rec->extended = extended;

    return rec;
}


/* ****** Distributor-Related Management ****** */

static inline void
__init_distributor(struct gic_distributor* d, 
                   gicreg_t* base, unsigned int nr_ints)
{
    bitmap_init_ptr(gic_bmp,
        &d->group, nr_ints, gic_regptr(base, GICD_IGROUPRn));

    bitmap_init_ptr(gic_bmp,
        &d->grpmod, nr_ints, gic_regptr(base, GICD_IGRPMODRn));

    bitmap_init_ptr(gic_bmp,
        &d->enable, nr_ints, gic_regptr(base, GICD_ISENABLER));
    
    bitmap_init_ptr(gic_bmp,
        &d->disable, nr_ints, gic_regptr(base, GICD_ICENABLER));

    bitmap_init_ptr(gic_bmp,
        &d->icfg, nr_ints * 2, gic_regptr(base, GICD_ICFGR));
    
    bitmap_init_ptr(gic_bmp,
        &d->nmi, nr_ints, gic_regptr(base, GICD_INMIR));
}

static inline struct leaflet*
__alloc_lpi_table(size_t table_sz)
{
    unsigned int val;
    struct leaflet* tab;

    val = page_aligned(table_sz);
    tab = alloc_leaflet(count_order(leaf_count(val)));
    leaflet_wipe(tab);

    return leaflet_addr(tab);
}

static inline void
__toggle_lpi_enable(struct gic_rd* rdist, bool en)
{
    gicreg_t val;

    if (!en) {
        while ((val = rdist->base[GICR_CTLR]) & GICR_CTLR_RWP);
        rdist->base[GICR_CTLR] = val & ~GICR_CTLR_EnLPI;
    }
    else {
        rdist->base[GICR_CTLR] = val | GICR_CTLR_EnLPI;
    }
}

static struct gic_distributor*
__attached_distributor(int cpu, struct gic_interrupt* ent)
{
    enum gic_int_type iclass;

    iclass = ent->config.class;

    if (iclass == GIC_PPI || iclass == GIC_SGI) {
        return &gic.pes[cpu].rdist;
    }
    
    if (ent->domain->extended) {
        return &gic.dist_e;
    }
    
    return &gic.dist;
}


/* ****** GIC Components Configuration ****** */

static void
gic_configure_icc()
{
    reg_t v;

    v =
    sysreg_flagging(ICC_SRE_EL1, 
                    ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB, 
                    0);
    

    v = 
    sysreg_flagging(ICC_CTLR_EL1,
                    ICC_CTRL_CBPR,
                    ICC_CTRL_EOImode | ICC_CTRL_PMHE);

    // disable all group 0 interrupts as those are meant for EL3
    v=
    sysreg_flagging(ICC_IGRPEN0_EL1, 0, ICC_IGRPEN_ENABLE);

    // enable all group 1 interrupts, we'll stick with EL1_NS
    v=
    sysreg_flagging(ICC_IGRPEN1_EL1, ICC_IGRPEN_ENABLE, 0);
}

static void
gic_configure_global(struct arm_gic* gic)
{
    gicreg64_t reg;
    unsigned int val, max_nr_spi;

    reg = gic->mmrs.dist_base[GICD_TYPER];
    
    // check if eSPI supported
    gic->has_espi = (reg & GICD_TYPER_ESPI);
    if (gic->has_espi) {
        val = BITS_GET(reg, GICD_TYPER_nESPI);
        gic->espi_nr = 32 * (val + 1);
    }

    // Parse IDbits
    val = BITS_GET(reg, GICD_TYPER_IDbits);
    gic->max_intid = 1 << (val + 1) - 1;

    // LPI is supported
    if (val + 1 >= 14) {
        val = BITS_GET(reg, GICD_TYPER_nLPI);
        if (val) {
            gic->lpi_nr = 1 << (val + 1);
        }
        else {
            gic->lpi_nr = gic->max_intid - INITID_LPI_BASE + 1;
        }
    }

    // check if SPI supported
    val = BITS_GET(reg, GICD_TYPER_nSPI);
    if (val) {
        max_nr_spi = 32 * (val + 1);
        gic->spi_nr = MIN(max_nr_spi, INITID_SPEC_BASE);
        gic->spi_nr -= INITID_SPI_BASE;
    } else {
        gic->spi_nr = 0;
    }

    gic->nmi_ready = (reg & GICD_TYPER_NMI);
    gic->msi_via_spi = (reg & GICD_TYPER_MBIS);

    __init_distributor(&gic->dist, gic->mmrs.dist_base, gic->spi_nr);
    __init_distributor(&gic->dist_e, gic->mmrs.dist_base, gic->espi_nr);


    if (gic->spi_nr) {
        gic->idomain.spi  = __idomain(gic->spi_nr, INITID_SPI_BASE, false);
    }
    if (gic->espi_nr) {
        gic->idomain.espi = __idomain(gic->espi_nr, INITID_eSPI_BASE, true);
    }
    if (gic->lpi_nr) {
        gic->idomain.lpi  = __idomain(gic->lpi_nr, INITID_LPI_BASE, false);
    }

    struct leaflet* tab;

    tab = __alloc_lpi_table(gic->lpi_nr);
    gic->lpi_tables.prop_table = vmap(tab, KERNEL_DATA);
    gic->lpi_tables.prop_pa = leaflet_addr(tab);

    tab = __alloc_lpi_table(gic->lpi_nr / 8);
    gic->lpi_tables.pend = leaflet_addr(tab);

    bitmap_init_ptr(gic_bmp, 
        &gic->lpi_tables.pendings, gic->lpi_nr, gic->lpi_tables.pend);
}

static void
gic_configure_pe(struct arm_gic* gic, struct gic_pe* pe)
{
    unsigned int nr_local_ints;
    gicreg64_t reg;

    reg = gic_reg64(pe->_rd, GICR_TYPER);

    pe->affinity = BITS_GET(reg, GICR_TYPER_AffVal);
    pe->ppi_nr   = INITID_PPI_BASE;
    switch (BITS_GET(reg, GICR_TYPER_PPInum))
    {
        case 1:
            pe->ppi_nr += 1088 - INITID_ePPI_BASE;
            pe->eppi_ready = true;
            break;
        case 2:
            pe->ppi_nr += 1120 - INITID_ePPI_BASE;
            pe->eppi_ready = true;
            break;
    }

    nr_local_ints = pe->ppi_nr + INITID_PPI_BASE;
    
    pe->idomain.local_ints = __idomain(32, 0, false);
    pe->idomain.eppi = __idomain(nr_local_ints - 32, INITID_ePPI_BASE, true);

    __init_distributor(&pe->rdist, pe->_rd->sgi_base, nr_local_ints);

    __toggle_lpi_enable(pe->_rd, false);

    reg = 0;
    BITS_SET(reg, GICR_BASER_PAddr, gic->lpi_tables.prop_pa);
    BITS_SET(reg, GICR_BASER_Share, 0b01);
    BITS_SET(reg, GICR_PROPBASER_IDbits, ilog2(gic->max_intid));
    pe->_rd->sgi_base[GICR_PROPBASER] = reg;

    reg  = 0;
    reg |= GICR_PENDBASER_PTZ;
    BITS_SET(reg, GICR_BASER_PAddr, gic->lpi_tables.pend);
    BITS_SET(reg, GICR_BASER_Share, 0b01);
    pe->_rd->sgi_base[GICR_PENDBASER] = reg;

    __toggle_lpi_enable(pe->_rd, true);
}


/* ****** Interrupt Life-cycle Management ****** */


struct gic_interrupt*
gic_install_int(struct gic_int_param* param, isr_cb handler, bool alloc)
{
    unsigned int iv;
    struct gic_idomain* domain;
    struct gic_interrupt* ent;
    struct gic_distributor* dist;
    int cpu;

    cpu = param->cpu_id;

    assert(cpu == 0);

    switch (param->class)
    {
    case GIC_PPI:
        if (!param->ext_range) {
            domain = gic.pes[cpu].idomain.local_ints;
        }
        else {
            domain = gic.pes[cpu].idomain.eppi;
        }
        break;

    case GIC_SGI:
        domain = gic.pes[cpu].idomain.local_ints;
        break;

    case GIC_SPI:
        if (!param->ext_range) {
            assert(gic.spi_nr > 0);
            domain = gic.idomain.spi;
        } 
        else {
            assert(gic.has_espi);
            domain = gic.idomain.espi;
        }
        break;

    case GIC_LPI:
        assert(gic.lpi_ready);
        domain = gic.idomain.lpi;
        break;
        
    default:
        fail("unknown interrupt class");
        break;
    }

    if (alloc) {
        if (!bitmap_alloc(gic_bmp, &domain->ivmap, 0, &iv)) {
            FATAL("out of usable iv for class=%d", param->class);
        }
    }
    else {
        iv = param->rel_intid;
        if ((ent = __get_interrupt(domain, iv))) {
            return ent;
        }
        
        bitmap_set(gic_bmp, &domain->ivmap, iv, true);
    }

    iv += domain->base;

    if (param->class == GIC_SPI && !param->ext_range && iv >= INITID_ePPI_BASE) 
    {
        WARN("PPI vector=%d falls in extended range, while not requested.", iv);
        param->ext_range = true;
    }

    ent  = __register_interrupt(domain, iv, param);
    dist = __attached_distributor(cpu, ent);
    
    __config_interrupt(&gic, dist, ent);

    ent->handler = handler;

    return ent;
}

static void
gic_update_active()
{
    reg_t val;
    unsigned int intid;
    struct gic_interrupt* intr;
    struct gic_pe* pe;

    pe  = &gic.pes[0];
    val = read_sysreg(ICC_IAR1_EL1);
    intid = (unsigned int)val & ((1 << 24) - 1);

    if (check_special_intid(intid)) {
        return;
    }

    intr = __find_interrupt_record(intid);
    pe->active = intr;
    pe->iar_val = val;
}

static inline void
gic_signal_eoi()
{
    struct gic_pe* pe;

    pe  = &gic.pes[0];
    if (!pe->active) {
        return;
    }

    pe->active = NULL;
    set_sysreg(ICC_EOIR1_EL1, pe->iar_val);
}

/* ****** Lunaix ISRM Interfacing ****** */

void
isrm_init()
{
    // nothing to do
}

void
isrm_ivfree(int iv)
{
    struct gic_interrupt* ent;
    struct gic_distributor* dist;
    
    ent  = __find_interrupt_record(iv);
    if (!ent) {
        return;
    }

    dist = __attached_distributor(0, ent);
    __undone_interrupt(&gic, dist, ent);

    hlist_delete(&ent->node);
    vfree(ent);
}

int
isrm_ivosalloc(isr_cb handler)
{
    return isrm_ivexalloc(handler);
}

int
isrm_ivexalloc(isr_cb handler)
{
    struct gic_int_param param;
    struct gic_interrupt* intr;

    param = (struct gic_int_param) {
        .class = GIC_SPI,
        .group = GIC_G1NS,
        .trigger = GIC_TRIG_EDGE,
    };

    intr = gic_install_int(&param, handler, true);
    
    return intr->intid;
}

isr_cb
isrm_get(int iv)
{
    struct gic_interrupt* intr;

    intr = __find_interrupt_record(iv);
    if (!intr) {
        return NULL;
    }

    return intr->handler;
}

ptr_t
isrm_get_payload(const struct hart_state* state)
{
    struct gic_interrupt* active;

    active = gic.pes[0].active;
    assert(active);

    return active->handler;
}

void
isrm_set_payload(int iv, ptr_t payload)
{
    struct gic_interrupt* intr;

    intr = __find_interrupt_record(iv);
    if (!intr) {
        return NULL;
    }

    intr->payload = payload;
}

void
isrm_notify_eoi(cpu_t id, int iv)
{
    gic_signal_eoi();
}

void
isrm_notify_eos(cpu_t id)
{
    isrm_notify_eoi(id, 0);
}

msi_vector_t
isrm_msialloc(isr_cb handler)
{
    int intid;
    msi_vector_t msiv;
    struct gic_int_param param;

    param = (struct gic_int_param) {
        .group = GIC_G1NS,
        .trigger = GIC_TRIG_EDGE
    };

    if (gic.msi_via_spi) {
        param.class = GIC_SPI;

        intid = gic_install_int(&param, handler, true);
        msiv.msi_addr  = gic_regptr(gic.mmrs.dist_base, GICD_SETSPI_NSR);
        goto done;
    }
    
    if (unlikely(!gic.lpi_ready)) {
        return invalid_msi_vector;
    }

    if (unlikely(llist_empty(&gic.its))) {
        // FIXME The MSI interface need rework
        WARN("ITS-base MSI is yet unsupported.");
        return invalid_msi_vector;
    }

    param.class = GIC_LPI;
    intid = gic_install_int(&param, handler, true);
    msiv.msi_addr = gic_regptr(gic.pes[0]._rd->base, GICR_SETLPIR);

done:
    msiv.mapped_iv = intid;
    msiv.msi_data  = intid;

    return msiv;
}

int
isrm_bind_dtnode(struct dt_intr_node* node, isr_cb handler)
{
    struct dt_prop_val* val;
    struct gic_int_param param;
    struct gic_interrupt* installed;

    val = dt_resolve_interrupt(INTR_TO_DTNODE(node));
    if (!val) {
        return EINVAL;
    }

    if (node->intr.extended) {
        WARN("binding of multi interrupt is yet to supported");
        return EINVAL;
    }

    gic_dtprop_interpret(&param, val, 3);
    param.cpu_id = 0;

    installed = gic_install_int(&param, handler, false);

    return installed->intid;
}

/* ****** Device Definition & Export ****** */

static void
gic_init()
{
    memset(&gic, 0, sizeof(gic));

    gic_create_from_dt(&gic);

    // configure the system interfaces
    gic_configure_icc();

    // configure global distributor
    gic_configure_global(&gic);
    
    // configure per-PE local distributor (redistributor)
    for (int i = 0; i < NR_CPU; i++)
    {
        gic_configure_pe(&gic, &gic.pes[i]);
    }

    gic_configure_its(&gic);
}

static struct device_def dev_arm_gic = {
    .name = "ARM Generic Interrupt Controller",
    .class = DEVCLASS(DEVIF_SOC, DEVFN_CFG, DEV_INTC),
    .init = gic_init
};
EXPORT_DEVICE(arm_gic, &dev_arm_gic, load_sysconf);