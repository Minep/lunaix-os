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

void
pci_probe_msi_info(struct pci_device* device);

static struct pci_device*
pci_create_device(ptr_t pci_base, int devinfo)
{
    pci_reg_t class = pci_read_cspace(pci_base, 0x8);
    struct hbucket* bucket = device_definitions_byif(DEVIF_PCI);

    u32_t devid = PCI_DEV_DEVID(devinfo);
    u32_t vendor = PCI_DEV_VENDOR(devinfo);

    kappendf(".%x:%x, ", vendor, devid);

    struct pci_device_def *pos, *n;
    hashtable_bucket_foreach(bucket, pos, n, devdef.hlist_if)
    {
        if (pos->dev_class != PCI_DEV_CLASS(class)) {
            continue;
        }

        int result = (pos->dev_vendor & vendor) == vendor &&
                     (pos->dev_id & devid) == devid;

        if (result) {
            goto found;
        }
    }

    kappendf(KWARN "unknown device\n");

    return NULL;

found:
    pci_reg_t intr = pci_read_cspace(pci_base, 0x3c);

    struct pci_device* device = vzalloc(sizeof(struct pci_device));
    device->class_info = class;
    device->device_info = devinfo;
    device->cspace_base = pci_base;
    device->intr_info = intr;

    device_prepare(&device->dev, &pos->devdef.class);

    pci_probe_msi_info(device);
    pci_probe_bar_info(device);

    kappendf("%s (dev.%x:%x:%x) \n",
             pos->devdef.name,
             pos->devdef.class.meta,
             pos->devdef.class.device,
             pos->devdef.class.variant);

    if (!pos->devdef.init_for) {
        kappendf(KERROR "bad def\n");
        goto fail;
    }

    int errno = pos->devdef.init_for(&pos->devdef, &device->dev);
    if (errno) {
        kappendf(KERROR "failed (e=%d)\n", errno);
        goto fail;
    }

    llist_append(&pci_devices, &device->dev_chain);

    return device;

fail:
    vfree(device);
    return NULL;
}

void
pci_probe_device(int bus, int dev, int funct)
{
    u32_t base = PCI_ADDRESS(bus, dev, funct);
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
    if ((hdr_type & 0x80) && funct == 0) {
        hdr_type = hdr_type & ~0x80;
        // 探测多用途设备（multi-function device）
        for (int i = 1; i < 7; i++) {
            pci_probe_device(bus, dev, i);
        }
    }

    if (hdr_type != PCI_TDEV) {
        // XXX: 目前忽略所有桥接设备，比如PCI-PCI桥接器，或者是CardBus桥接器
        return;
    }

    kprintf("pci.%d:%d:%d", bus, dev, funct);

    pci_create_device(base, reg1);
}

void
pci_scan()
{
    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            pci_probe_device(bus, dev, 0);
        }
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

void
pci_build_fsmapping()
{
    struct twifs_node *pci_class = twifs_dir_node(NULL, "pci"), *pci_dev;
    struct pci_device *pos, *n;
    struct twimap* map;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        pci_dev = twifs_dir_node(pci_class,
                                 "%.2d:%.2d:%.2d.%.4x:%.4x",
                                 PCI_BUS_NUM(pos->cspace_base),
                                 PCI_SLOT_NUM(pos->cspace_base),
                                 PCI_FUNCT_NUM(pos->cspace_base),
                                 PCI_DEV_VENDOR(pos->device_info),
                                 PCI_DEV_DEVID(pos->device_info));

        map = twifs_mapping(pci_dev, pos, "config");
        map->read = __pci_read_cspace;

        map = twifs_mapping(pci_dev, pos, "revision");
        map->read = __pci_read_revid;

        map = twifs_mapping(pci_dev, pos, "class");
        map->read = __pci_read_class;

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
    pci_scan();

    return 0;
}

static struct device_def pci_def = {
    .name = "pci3.0-hba",
    .class = DEVCLASS(DEVIF_SOC, DEVFN_BUSIF, DEV_BUS, 0),
    .init = pci_load_devices
};
EXPORT_DEVICE(pci3hba, &pci_def, load_poststage);
