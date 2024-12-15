/**
 * @file pci.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief A software implementation of PCI Local Bus Specification Revision 3.0
 * @version 0.1
 * @date 2022-06-28
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <hal/pci.h>

#include <klibc/string.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("PCI")

/*
    device instance for pci bridge, 
    currently, lunaix only support one bridge controller.
*/
static struct device* pci_bridge;

static morph_t* pci_probers;
static DECLARE_HASHTABLE(pci_drivers, 8);

static inline void
pci_log_device(struct pci_probe* probe)
{
    pciaddr_t loc = probe->loc;

    kprintf("pci.%03d:%02d:%02d, class=%p, vendor:dev=%04x:%04x",
            PCILOC_BUS(loc),
            PCILOC_DEV(loc),
            PCILOC_FN(loc),
            probe->class_info,
            PCI_DEV_VENDOR(probe->device_info),
            PCI_DEV_DEVID(probe->device_info));
}

static void
__pci_probe_msi_info(struct pci_probe* probe)
{
    // Note that Virtualbox have to use ICH9 chipset for MSI support.
    // Qemu seems ok with default PIIX3, Bochs is pending to test...
    //    See https://www.virtualbox.org/manual/ch03.html (section 3.5.1)
    pci_reg_t status =
      pci_read_cspace(probe->cspace_base, PCI_REG_STATUS_CMD) >> 16;

    if (!(status & 0x10)) {
        probe->msi_loc = 0;
        return;
    }

    pci_reg_t cap_ptr = pci_read_cspace(probe->cspace_base, 0x34) & 0xff;
    u32_t cap_hdr;

    while (cap_ptr) {
        cap_hdr = pci_read_cspace(probe->cspace_base, cap_ptr);
        if ((cap_hdr & 0xff) == 0x5) {
            // MSI
            probe->msi_loc = cap_ptr;
            return;
        }
        cap_ptr = (cap_hdr >> 8) & 0xff;
    }
}


static void
__pci_probe_bar_info(struct pci_probe* probe)
{
    u32_t bar;
    struct pci_base_addr* ba;
    for (size_t i = 0; i < PCI_BAR_COUNT; i++) {
        ba = &probe->bar[i];
        ba->size = pci_bar_sizing(probe, &bar, i + 1);
        if (PCI_BAR_MMIO(bar)) {
            ba->start = PCI_BAR_ADDR_MM(bar);
            ba->type |= PCI_BAR_CACHEABLE(bar) ? BAR_TYPE_CACHABLE : 0;
            ba->type |= BAR_TYPE_MMIO;
        } else {
            ba->start = PCI_BAR_ADDR_IO(bar);
        }
    }
}

static void
__pci_add_prober(pciaddr_t loc, ptr_t pci_base, int devinfo)
{
    struct pci_probe* prober;
    morph_t* probe_morph;

    pci_reg_t class = pci_read_cspace(pci_base, 0x8);

    u32_t devid = PCI_DEV_DEVID(devinfo);
    u32_t vendor = PCI_DEV_VENDOR(devinfo);
    pci_reg_t intr = pci_read_cspace(pci_base, 0x3c);

    prober = vzalloc(sizeof(*prober));

    prober->class_info = class;
    prober->device_info = devinfo;
    prober->cspace_base = pci_base;
    prober->intr_info = intr;
    prober->loc = loc;
    prober->irq_domain = irq_get_domain(pci_bridge);

    changeling_morph_anon(pci_probers, prober->mobj, pci_probe_morpher);

    __pci_probe_msi_info(prober);
    __pci_probe_bar_info(prober);

    pci_log_device(prober);
}

static int
__pci_bind(struct pci_registry* reg, struct pci_probe* probe)
{
    int errno;
    struct device_def* devdef;

    if (probe->bind) {
        return EEXIST;
    }

    if (!reg->check_compact(probe)) {
        return EINVAL;
    }

    devdef = reg->definition;
    errno = devdef->create(devdef, &probe->mobj);

    if (errno) {
        ERROR("pci_loc:%x, bind (%xh:%xh.%d) failed, e=%d",
                probe->loc,
                devdef->class.fn_grp,
                devdef->class.device,
                devdef->class.variant,
                errno);
    }

    return errno;
}

int
pci_bind_driver(struct pci_registry* reg)
{
    struct pci_probe* probe;
    int errno = 0;
    
    morph_t *pos, *n;
    changeling_for_each(pos, n, pci_probers)
    {
        probe = changeling_reveal(pos, pci_probe_morpher);

        errno = __pci_bind(reg, probe);
        if (errno) {
            continue;
        }
    }

    return errno;
}

void
pci_probe_device(pciaddr_t pci_loc)
{
    u32_t base = PCI_CFGADDR(pci_loc);
    pci_reg_t reg1 = pci_read_cspace(base, 0);

    // Vendor=0xffff则表示设备不存在
    if (PCI_DEV_VENDOR(reg1) == PCI_VENDOR_INVLD) {
        return;
    }

    pci_reg_t hdr_type = pci_read_cspace(base, 0xc);
    hdr_type = (hdr_type >> 16) & 0xff;

    // 防止堆栈溢出
    // QEMU的ICH9/Q35实现似乎有点问题，对于多功能设备的每一个功能的header type
    //  都将第七位置位。而virtualbox 就没有这个毛病。
    if ((hdr_type & 0x80) && PCILOC_FN(pci_loc) == 0) {
        hdr_type = hdr_type & ~0x80;
        // 探测多用途设备（multi-function device）
        for (int i = 1; i < 7; i++) {
            pci_probe_device(pci_loc + i);
        }
    }

    struct pci_probe* prober;
    morph_t *pos, *n;
    changeling_for_each(pos, n, pci_probers) 
    {
        prober = changeling_reveal(pos, pci_probe_morpher);
        if (prober->loc == pci_loc) {
            pci_log_device(prober);
            return;
        }
    }

    __pci_add_prober(pci_loc, base, reg1);
}

static struct pci_registry*
__pci_registry_get(struct device_def* def)
{
    struct pci_registry *pos, *n;
    unsigned int hash;

    hash = hash_32(__ptr(def), HSTR_FULL_HASH);
    hashtable_hash_foreach(pci_drivers, hash, pos, n, entries)
    {
        if (pos->definition == def) {
            return pos;
        }
    }

    return NULL;
}

static int
__pci_proxied_devdef_load(struct device_def* def)
{
    struct pci_registry* reg;
    int errno = 0;

    reg = __pci_registry_get(def);
    assert(reg);

    return pci_bind_driver(reg);
}

bool
pci_register_driver(struct device_def* def, pci_id_checker_t checker)
{
    struct pci_registry* reg;
    unsigned int hash;

    if (!checker) {
        return false;
    }

    if (__pci_registry_get(def)) {
        return false;
    }

    reg = valloc(sizeof(*reg));
    
    *reg = (struct pci_registry) {
        .check_compact = checker,
        .definition = def,
    };

    device_chain_loader(def, __pci_proxied_devdef_load);

    hash = hash_32(__ptr(def), HSTR_FULL_HASH);
    hashtable_hash_in(pci_drivers, &reg->entries, hash);

    return true;
}

void
pci_scan()
{
    for (u32_t loc = 0; loc < (pciaddr_t)-1; loc += 8) {
        pci_probe_device((pciaddr_t)loc);
    }
}

static void
__pci_config_msi(struct pci_probe* probe, irq_t irq)
{
    // PCI LB Spec. (Rev 3) Section 6.8 & 6.8.1

    ptr_t msi_addr = irq->msi->wr_addr;
    u32_t msi_data = irq->msi->message;

    pci_reg_t reg1 = pci_read_cspace(probe->cspace_base, probe->msi_loc);
    pci_reg_t msg_ctl = reg1 >> 16;
    int offset_cap64 = !!(msg_ctl & MSI_CAP_64BIT) * 4;

    pci_write_cspace(probe->cspace_base, 
                     PCI_MSI_ADDR_LO(probe->msi_loc), 
                     msi_addr);
    
    if (offset_cap64) {
        pci_write_cspace(probe->cspace_base, 
                         PCI_MSI_ADDR_HI(probe->msi_loc), 
                         (u64_t)msi_addr >> 32);
    }

    pci_write_cspace(probe->cspace_base,
                     PCI_MSI_DATA(probe->msi_loc, offset_cap64),
                     msi_data & 0xffff);

    if ((msg_ctl & MSI_CAP_MASK)) {
        pci_write_cspace(
          probe->cspace_base, PCI_MSI_MASK(probe->msi_loc, offset_cap64), 0);
    }

    // manipulate the MSI_CTRL to allow device using MSI to request service.
    reg1 = (reg1 & 0xff8fffff) | 0x10000;
    pci_write_cspace(probe->cspace_base, probe->msi_loc, reg1);
}

irq_t
pci_declare_msi_irq(irq_servant callback, struct pci_probe* probe)
{
    return irq_declare_msg(callback, probe->loc, probe->loc);
}

int
pci_assign_msi(struct pci_probe* probe, irq_t irq, void* irq_spec)
{
    int err = 0;

    assert(irq->type == IRQ_MESSAGE);

    err = irq_assign(probe->irq_domain, irq, irq_spec);
    if (err) {
        return err;
    }

    __pci_config_msi(probe, irq);
    return 0;
}

size_t
pci_bar_sizing(struct pci_probe* probe, u32_t* bar_out, u32_t bar_num)
{
    pci_reg_t sized, bar;
    
    bar = pci_read_cspace(probe->cspace_base, PCI_REG_BAR(bar_num));
    if (!bar) {
        *bar_out = 0;
        return 0;
    }

    pci_write_cspace(probe->cspace_base, PCI_REG_BAR(bar_num), 0xffffffff);
    
    sized =
      pci_read_cspace(probe->cspace_base, PCI_REG_BAR(bar_num)) & ~0x1;
    
    if (PCI_BAR_MMIO(bar)) {
        sized = PCI_BAR_ADDR_MM(sized);
    }
    
    *bar_out = bar;
    pci_write_cspace(probe->cspace_base, PCI_REG_BAR(bar_num), bar);
    
    return ~sized + 1;
}

void
pci_apply_command(struct pci_probe* probe, pci_reg_t cmd)
{
    pci_reg_t rcmd;
    ptr_t base;

    base = probe->cspace_base;
    rcmd = pci_read_cspace(base, PCI_REG_STATUS_CMD);

    cmd  = cmd & 0xffff;
    rcmd = (rcmd & 0xffff0000) | cmd;

    pci_write_cspace(base, PCI_REG_STATUS_CMD, rcmd);
}

static void
__pci_read_cspace(struct twimap* map)
{
    struct pci_probe* probe;

    probe = twimap_data(map, struct pci_probe*);

    for (size_t i = 0; i < 256; i += sizeof(pci_reg_t)) {
        *(pci_reg_t*)(map->buffer + i) =
          pci_read_cspace(probe->cspace_base, i);
    }

    map->size_acc = 256;
}

/*---------- TwiFS interface definition ----------*/

static void
__pci_read_revid(struct twimap* map)
{
    struct pci_probe* probe;

    probe = twimap_data(map, struct pci_probe*);
    twimap_printf(map, "0x%x", PCI_DEV_REV(probe->class_info));
}

static void
__pci_read_class(struct twimap* map)
{
    struct pci_probe* probe;

    probe = twimap_data(map, struct pci_probe*);
    twimap_printf(map, "0x%x", PCI_DEV_CLASS(probe->class_info));
}

static void
__pci_read_devinfo(struct twimap* map)
{
    struct pci_probe* probe;

    probe = twimap_data(map, struct pci_probe*);
    twimap_printf(map, "%x:%x", 
            PCI_DEV_VENDOR(probe->device_info), 
            PCI_DEV_DEVID(probe->device_info));
}

static void
__pci_bar_read(struct twimap* map)
{
    struct pci_probe* probe;
    int bar_index;

    probe = twimap_data(map, struct pci_probe*);
    bar_index = twimap_index(map, int);

    struct pci_base_addr* bar = &probe->bar[bar_index];

    if (!bar->start && !bar->size) {
        twimap_printf(map, "[%d] not present \n", bar_index);
        return;
    }

    twimap_printf(
      map, "[%d] base=%.8p, size=%.8p, ", bar_index, bar->start, bar->size);

    if ((bar->type & BAR_TYPE_MMIO)) {
        twimap_printf(map, "mmio");
        if ((bar->type & BAR_TYPE_CACHABLE)) {
            twimap_printf(map, ", prefetchable");
        }
    } else {
        twimap_printf(map, "io");
    }

    twimap_printf(map, "\n");
}

static int
__pci_bar_gonext(struct twimap* map)
{
    if (twimap_index(map, int) >= 5) {
        return 0;
    }
    map->index += 1;
    return 1;
}

static void
__pci_read_binding(struct twimap* map)
{
    struct pci_probe* probe;
    struct devident* devid;

    probe = twimap_data(map, struct pci_probe*);
    if (!probe->bind) {
        return;
    }

    devid = &probe->bind->ident;

    twimap_printf(map, "%xh:%xh.%d",
                  devid->fn_grp,
                  DEV_KIND_FROM(devid->unique),
                  DEV_VAR_FROM(devid->unique));
}

static void
__pci_trigger_bus_rescan(struct twimap* map)
{
    pci_scan();
}

void
pci_build_fsmapping()
{
    struct twifs_node *pci_class = twifs_dir_node(NULL, "pci"), *pci_dev;
    struct twimap* map;
    struct pci_probe* probe;
    morph_t *pos, *n;

    // TODO bus rescan is not ready yet
    // map = twifs_mapping(pci_class, NULL, "rescan");
    // map->read = __pci_trigger_bus_rescan;

    changeling_for_each(pos, n, pci_probers)
    {
        probe = changeling_reveal(pos, pci_probe_morpher);
        pci_dev = twifs_dir_node(pci_class, "%x", probe->loc);

        map = twifs_mapping(pci_dev, probe, "config");
        map->read = __pci_read_cspace;

        map = twifs_mapping(pci_dev, probe, "revision");
        map->read = __pci_read_revid;

        map = twifs_mapping(pci_dev, probe, "class");
        map->read = __pci_read_class;

        map = twifs_mapping(pci_dev, probe, "binding");
        map->read = __pci_read_binding;

        map = twifs_mapping(pci_dev, probe, "io_bases");
        map->read = __pci_bar_read;
        map->go_next = __pci_bar_gonext;
    }
}
EXPORT_TWIFS_PLUGIN(pci_devs, pci_build_fsmapping);

/*---------- PCI 3.0 HBA device definition ----------*/

static int
__pci_irq_install(struct irq_domain* domain, irq_t irq)
{
    struct irq_domain* parent;
    int err;

    parent = domain->parent;
    err = parent->ops->install_irq(parent, irq);
    if (err) {
        return err;
    }

    if (irq->type == IRQ_MESSAGE) {
        irq->msi->message = irq->vector;
    }

    return 0;
}

static struct irq_domain_ops pci_irq_ops = {
    .install_irq = __pci_irq_install
};

static int
pci_register(struct device_def* def)
{
    pci_probers = changeling_spawn(NULL, "pci_realm");

    return 0;
}

static int
pci_create(struct device_def* def, morph_t* obj)
{
    struct irq_domain *pci_domain;
    pci_bridge = device_allocsys(NULL, NULL);

#ifdef CONFIG_USE_DEVICETREE
    devtree_link_t devtree_node;
    devtree_node = changeling_try_reveal(obj, dt_morpher);
    device_set_devtree_node(pci_bridge, devtree_node);
#endif

    pci_domain = irq_create_domain(pci_bridge, &pci_irq_ops);
    irq_attach_domain(irq_get_default_domain(), pci_domain);

    register_device(pci_bridge, &def->class, "pci_bridge");
    pci_scan();

    return 0;
}

static struct device_def pci_def = {
    def_device_name("Generic PCI"),
    def_device_class(GENERIC, BUSIF, PCI),

    def_on_register(pci_register),
    def_on_create(pci_create)
};
EXPORT_DEVICE(pci3hba, &pci_def, load_onboot);
