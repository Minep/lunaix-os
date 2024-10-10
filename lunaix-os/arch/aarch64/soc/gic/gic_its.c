#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>

#include <asm/soc/gic.h>

struct gic_its*
gic_its_create(struct arm_gic* gic, ptr_t regs)
{
    struct gic_its* its;

    its = valloc(sizeof(*its));

    its->reg_ptr       = regs;
    its->cmd_queue_ptr = gic_regptr(its->reg->base, GITS_CBASER);
    its->table_ptr     = gic_regptr(its->reg->base, GITS_BASER);

    llist_append(&gic->its, &its->its);
    hashtable_init(its->devmaps);

    return its;
}

ptr_t
__pack_base_reg(struct leaflet* page, int ic, int oc)
{
    ptr_t reg = 0;
    
    reg |= GITS_BASER_VALID;
    BITS_SET(reg, GITS_BASER_ICACHE, ic);
    BITS_SET(reg, GITS_BASER_OCACHE, oc);
    BITS_SET(reg, GITS_BASER_PA, leaflet_addr(page));
    BITS_SET(reg, GITS_BASER_SIZE, 1);
    BITS_SET(reg, GITS_BASER_SHARE, 0b01);

    return reg;
}

static void
__configure_table_desc(struct gic_its* its, unsigned int off)
{
    gicreg_t val;
    unsigned int type, sz;

    val = its->tables->base[off];
    type = BITS_GET(val, GITS_BASERn_TYPE);
    sz   = BITS_GET(val, GITS_BASERn_EntSz) + 1;

    switch (type)
    {
        case 0b001: // DeviceID
            its->nr_devid = PAGE_SIZE / sz;
            break;
        case 0b100: // CollectionID
            its->nr_cid = PAGE_SIZE / sz;
            break;
        default:
            return;
    }

    val = __pack_base_reg(alloc_leaflet(0), 0, 0);
    BITS_SET(val, GITS_BASERn_TYPE, type);
    BITS_SET(val, GITS_BASERn_EntSz, sz);
    BITS_SET(val, GITS_BASERn_PGSZ,  0);    // 4K

    its->tables->base[off] = val;
}

static void
__configure_one_its(struct arm_gic* gic, struct gic_its* its)
{
    struct leaflet *leaflet;
    struct gic_its_regs *reg;
    gicreg_t val;
    unsigned int field_val;

    reg = its->reg;
    wait_until(!(val = reg->base[GITS_CTLR]) & GITS_CTLR_QS);
    reg->base[GITS_CTLR] = val & ~GITS_CTLR_EN;

    val = reg->base[GITS_TYPER];

    field_val = BITS_GET(val, GITS_TYPER_Devbits);
    its->nr_devid = 1 << (field_val + 1);

    field_val = BITS_GET(val, GITS_TYPER_ID_bits);
    its->nr_evtid = 1 << (field_val + 1);

    field_val = BITS_GET(val, GITS_TYPER_ITTe_sz);
    its->nr_evtid = field_val + 1;

    if (!(val & GITS_TYPER_CIL)) {
        its->nr_cid = BITS_GET(val, GITS_TYPER_HCC);
    }
    else {
        field_val = BITS_GET(val, GITS_TYPER_CIDbits);
        its->nr_cid = field_val + 1;
        its->ext_cids = true;
    }

    its->pta = !!(val & GITS_TYPER_PTA);

    leaflet = alloc_leaflet_pinned(0);
    val = __pack_base_reg(leaflet, 0, 0);
    its->cmd_queue->base = val;
    its->cmd_queue->wr_ptr = 0;
    its->cmds_ptr = vmap(leaflet, KERNEL_DATA);
    its->max_cmd  = leaflet_size(leaflet) / sizeof(struct gic_its_cmd);

    // total of 8 GITS_BASER<n> registers
    for (int i = 0; i < 8; i++)
    {
        __configure_table_desc(its, i);
    }

    reg->base[GITS_CTLR] |= GITS_CTLR_EN;
}

static void
__submit_itscmd(struct gic_its* its, struct gic_its_cmd* cmd)
{
    gicreg_t reg;
    ptr_t wrp;

    reg = its->cmd_queue->wr_ptr;

    wrp = BITS_GET(reg, GITS_CWRRD_OFF);

    if (wrp == its->max_cmd) {
        /*
            it is very unlikely we will submit the commands fast
            enough to satiate the entire queue. so we just roll
            back to front, assume the ITS always keeping up.
        */
        wrp = 0;
    }

    its->cmds[wrp++] = *cmd;

    reg = BITS_SET(reg, GITS_CWRRD_OFF, wrp);
    its->cmd_queue->wr_ptr = reg;
}

static void
__build_mapc(struct gic_its *its, struct gic_its_cmd* cmd, 
             struct gic_rd* rd, unsigned int cid)
{
    cmd->dw[0] = 0x09;
    cmd->dw[1] = 0;
    cmd->dw[3] = 0;
    cmd->dw[2] = (1UL << 63) | 
                 (__ptr(rd) & ~0xffff) | 
                 (cid & 0xffff);
}

static void
__build_mapd(struct gic_its *its, struct gic_its_cmd* cmd, 
             struct leaflet *itt_page, unsigned int devid)
{
    cmd->dw[0] = ((gicreg64_t)devid << 32) | 0x08;
    cmd->dw[1] = ilog2(its->nr_evtid);
    cmd->dw[3] = 0;
    cmd->dw[2] = (1UL << 63) | 
                 (leaflet_addr(itt_page));
}

static void
__build_mapti(struct gic_its *its, struct gic_its_cmd* cmd, 
              unsigned int devid, unsigned int evtid, 
              unsigned int lpid, unsigned int cid)
{
    cmd->dw[0] = ((gicreg64_t)devid << 32) | 0x0A;
    cmd->dw[1] = ((gicreg64_t)evtid << 32) | evtid;
    cmd->dw[3] = 0;
    cmd->dw[2] = (cid & 0xffff);
}

static unsigned int
__alloc_evtid(struct gic_its *its, unsigned int devid)
{
    unsigned int evtid;
    struct gic_its_devmap *pos, *n;
    struct gic_its_cmd cmd;

    hashtable_hash_foreach(its->devmaps, devid, pos, n, node)
    {
        if (pos->devid == devid) {
            goto found;
        }
    }

    pos = valloc(sizeof(*pos));
    pos->next_evtid = 0;
    pos->devid = devid;
    hashtable_hash_in(its->devmaps, &pos->node, devid);

    __build_mapd(its, &cmd, alloc_leaflet_pinned(0), devid);
    __submit_itscmd(its, &cmd);

found:
    evtid = pos->next_evtid++;

    assert(pos->next_evtid < its->nr_evtid);

    return evtid;
}

unsigned int
gic_its_map_lpi(struct gic_its* its, 
                unsigned int devid, unsigned int lpid)
{
    unsigned int evtid;
    struct gic_its_cmd cmd;

    evtid = __alloc_evtid(its, devid);

    __build_mapti(its, &cmd, devid, evtid, lpid, 0);
    __submit_itscmd(its, &cmd);

    return evtid;
}

void
gic_configure_its(struct arm_gic* gic)
{
    struct gic_its *pos, *n;
    struct gic_its_cmd cmd;
    
    llist_for_each(pos, n, &gic->its, its)
    {
        __configure_one_its(gic, pos);
    }

    // halt the cpu for a while to let ITS warming up
    wait_until_expire(true, 10000);

    // identity map every re-distributor to collection
    llist_for_each(pos, n, &gic->its, its)
    {
        for (int i = 0; i < NR_CPU; i++)
        {
            __build_mapc(pos, &cmd, gic->pes[i]._rd, i);
            __submit_itscmd(pos, &cmd);
        }
    }
}