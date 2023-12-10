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
#include <sys/pci_hba.h>

#include <klibc/string.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("PCI")

static DEFINE_LLIST(pci_devices);
static DECLARE_HASHTABLE(pci_devcache, 8);

static struct device_cat* pcidev_cat;
static struct device_def pci_def;

void
pci_probe_msi_info(struct pci_device* device);

static inline void
pci_log_device(struct pci_device* pcidev)
{
    pciaddr_t loc = pcidev->loc;
    struct device_def* binddef = pcidev->binding.def;

    kprintf("pci.%03d:%02d:%02d, class=%p, vendor:dev=%04x:%04x",
            PCILOC_BUS(loc),
            PCILOC_DEV(loc),
            PCILOC_FN(loc),
            pcidev->class_info,
            PCI_DEV_VENDOR(pcidev->device_info),
            PCI_DEV_DEVID(pcidev->device_info));
}

static struct pci_device*
pci_create_device(pciaddr_t loc, ptr_t pci_base, int devinfo)
{
    pci_reg_t class = pci_read_cspace(pci_base, 0x8);

    u32_t devid = PCI_DEV_DEVID(devinfo);
    u32_t vendor = PCI_DEV_VENDOR(devinfo);
    pci_reg_t intr = pci_read_cspace(pci_base, 0x3c);

    struct pci_device* device = vzalloc(sizeof(struct pci_device));
    device->class_info = class;
    device->device_info = devinfo;
    device->cspace_base = pci_base;
    device->intr_info = intr;

    device_create(&device->dev, dev_meta(pcidev_cat), DEV_IFSYS, NULL);

    pci_probe_msi_info(device);
    pci_probe_bar_info(device);

    llist_append(&pci_devices, &device->dev_chain);
    register_device(&device->dev, &pci_def.class, "%x", loc);
    pci_def.class.variant++;

    return device;
}

int
pci_bind_definition(struct pci_device_def* pcidev_def, int* more)
{
    u32_t class = pcidev_def->dev_class;
    u32_t devid_mask = pcidev_def->ident_mask;
    u32_t devid = pcidev_def->dev_ident & devid_mask;

    if (!pcidev_def->devdef.bind) {
        ERROR("pcidev %xh:%xh.%d is unbindable",
              pcidev_def->devdef.class.fn_grp,
              pcidev_def->devdef.class.device,
              pcidev_def->devdef.class.variant);
        return EINVAL;
    }

    *more = 0;

    int bind_attempted = 0;
    int errno = 0;

    struct device_def* devdef;
    struct pci_device *pos, *n;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        if (binded_pcidev(pos)) {
            continue;
        }

        if (class != PCI_DEV_CLASS(pos->class_info)) {
            continue;
        }

        int matched = (pos->device_info & devid_mask) == devid;

        if (!matched) {
            continue;
        }

        if (bind_attempted) {
            *more = 1;
            break;
        }

        bind_attempted = 1;
        devdef = &pcidev_def->devdef;
        errno = devdef->bind(devdef, &pos->dev);

        if (errno) {
            ERROR("pci_loc:%x, bind (%xh:%xh.%d) failed, e=%d",
                  pos->loc,
                  devdef->class.fn_grp,
                  devdef->class.device,
                  devdef->class.variant,
                  errno);
            continue;
        }

        pos->binding.def = &pcidev_def->devdef;
    }

    return errno;
}

int
pci_bind_definition_all(struct pci_device_def* pcidef)
{
    int more = 0, e = 0;
    do {
        if (!(e = pci_bind_definition(pcidef, &more))) {
            break;
        }
    } while (more);

    return e;
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

    struct pci_device *pos, *n;
    hashtable_hash_foreach(pci_devcache, pci_loc, pos, n, dev_cache)
    {
        if (pos->loc == pci_loc) {
            pci_log_device(pos);
            return;
        }
    }

    struct pci_device* pcidev = pci_create_device(pci_loc, base, reg1);
    if (pcidev) {
        pcidev->loc = pci_loc;
        hashtable_hash_in(pci_devcache, &pcidev->dev_cache, pci_loc);
        pci_log_device(pcidev);
    }
}

void
pci_scan()
{
    for (u32_t loc = 0; loc < (pciaddr_t)-1; loc += 8) {
        pci_probe_device((pciaddr_t)loc);
    }
}

void
pci_probe_bar_info(struct pci_device* device)
{
    u32_t bar;
    struct pci_base_addr* ba;
    for (size_t i = 0; i < 6; i++) {
        ba = &device->bar[i];
        ba->size = pci_bar_sizing(device, &bar, i + 1);
        if (PCI_BAR_MMIO(bar)) {
            ba->start = PCI_BAR_ADDR_MM(bar);
            ba->type |= PCI_BAR_CACHEABLE(bar) ? BAR_TYPE_CACHABLE : 0;
            ba->type |= BAR_TYPE_MMIO;
        } else {
            ba->start = PCI_BAR_ADDR_IO(bar);
        }
    }
}

void
pci_probe_msi_info(struct pci_device* device)
{
    // Note that Virtualbox have to use ICH9 chipset for MSI support.
    // Qemu seems ok with default PIIX3, Bochs is pending to test...
    //    See https://www.virtualbox.org/manual/ch03.html (section 3.5.1)
    pci_reg_t status =
      pci_read_cspace(device->cspace_base, PCI_REG_STATUS_CMD) >> 16;

    if (!(status & 0x10)) {
        device->msi_loc = 0;
        return;
    }

    pci_reg_t cap_ptr = pci_read_cspace(device->cspace_base, 0x34) & 0xff;
    u32_t cap_hdr;

    while (cap_ptr) {
        cap_hdr = pci_read_cspace(device->cspace_base, cap_ptr);
        if ((cap_hdr & 0xff) == 0x5) {
            // MSI
            device->msi_loc = cap_ptr;
            return;
        }
        cap_ptr = (cap_hdr >> 8) & 0xff;
    }
}

size_t
pci_bar_sizing(struct pci_device* dev, u32_t* bar_out, u32_t bar_num)
{
    pci_reg_t bar = pci_read_cspace(dev->cspace_base, PCI_REG_BAR(bar_num));
    if (!bar) {
        *bar_out = 0;
        return 0;
    }

    pci_write_cspace(dev->cspace_base, PCI_REG_BAR(bar_num), 0xffffffff);
    pci_reg_t sized =
      pci_read_cspace(dev->cspace_base, PCI_REG_BAR(bar_num)) & ~0x1;
    if (PCI_BAR_MMIO(bar)) {
        sized = PCI_BAR_ADDR_MM(sized);
    }
    *bar_out = bar;
    pci_write_cspace(dev->cspace_base, PCI_REG_BAR(bar_num), bar);
    return ~sized + 1;
}

struct pci_device*
pci_get_device_by_id(u16_t vendorId, u16_t deviceId)
{
    u32_t dev_info = vendorId | (deviceId << 16);
    struct pci_device *pos, *n;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        if (pos->device_info == dev_info) {
            return pos;
        }
    }

    return NULL;
}

struct pci_device*
pci_get_device_by_class(u32_t class)
{
    struct pci_device *pos, *n;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        if (PCI_DEV_CLASS(pos->class_info) == class) {
            return pos;
        }
    }

    return NULL;
}

static void
__pci_read_cspace(struct twimap* map)
{
    struct pci_device* pcidev = (struct pci_device*)(map->data);

    for (size_t i = 0; i < 256; i += sizeof(pci_reg_t)) {
        *(pci_reg_t*)(map->buffer + i) =
          pci_read_cspace(pcidev->cspace_base, i);
    }

    map->size_acc = 256;
}

/*---------- TwiFS interface definition ----------*/

static void
__pci_read_revid(struct twimap* map)
{
    int class = twimap_data(map, struct pci_device*)->class_info;
    twimap_printf(map, "0x%x", PCI_DEV_REV(class));
}

static void
__pci_read_class(struct twimap* map)
{
    int class = twimap_data(map, struct pci_device*)->class_info;
    twimap_printf(map, "0x%x", PCI_DEV_CLASS(class));
}

static void
__pci_read_devinfo(struct twimap* map)
{
    int devinfo = twimap_data(map, struct pci_device*)->device_info;
    twimap_printf(
      map, "%x:%x", PCI_DEV_VENDOR(devinfo), PCI_DEV_DEVID(devinfo));
}

static void
__pci_bar_read(struct twimap* map)
{
    struct pci_device* pcidev = twimap_data(map, struct pci_device*);
    int bar_index = twimap_index(map, int);

    struct pci_base_addr* bar = &pcidev->bar[bar_index];

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
    struct pci_device* pcidev = twimap_data(map, struct pci_device*);
    struct device_def* devdef = pcidev->binding.def;
    if (!devdef) {
        return;
    }

    twimap_printf(map,
                  "%xh:%xh.%d",
                  devdef->class.fn_grp,
                  devdef->class.device,
                  devdef->class.variant);
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
    struct pci_device *pos, *n;
    struct twimap* map;

    map = twifs_mapping(pci_class, NULL, "rescan");
    map->read = __pci_trigger_bus_rescan;

    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        pci_dev = twifs_dir_node(pci_class, "%x", pos->loc);

        map = twifs_mapping(pci_dev, pos, "config");
        map->read = __pci_read_cspace;

        map = twifs_mapping(pci_dev, pos, "revision");
        map->read = __pci_read_revid;

        map = twifs_mapping(pci_dev, pos, "class");
        map->read = __pci_read_class;

        map = twifs_mapping(pci_dev, pos, "binding");
        map->read = __pci_read_binding;

        map = twifs_mapping(pci_dev, pos, "io_bases");
        map->read = __pci_bar_read;
        map->go_next = __pci_bar_gonext;
    }
}
EXPORT_TWIFS_PLUGIN(pci_devs, pci_build_fsmapping);

/*---------- PCI 3.0 HBA device definition ----------*/

static int
pci_load_devices(struct device_def* def)
{
    pcidev_cat = device_addcat(NULL, "pci");

    pci_scan();

    return 0;
}

void
pci_bind_instance(struct pci_device* pcidev, void* devobj)
{
    pcidev->dev.underlay = devobj;
    pcidev->binding.dev = devobj;
}

static struct device_def pci_def = {
    .name = "Generic PCI",
    .class = DEVCLASS(DEVIF_SOC, DEVFN_BUSIF, DEV_PCI),
    .init = pci_load_devices
};
EXPORT_DEVICE(pci3hba, &pci_def, load_sysconf);
