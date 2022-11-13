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
#include <hal/acpi/acpi.h>
#include <hal/apic.h>
#include <hal/pci.h>
#include <klibc/string.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("PCI")

static DEFINE_LLIST(pci_devices);
static DEFINE_LLIST(pci_drivers);

void
pci_probe_msi_info(struct pci_device* device);

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

    pci_reg_t intr = pci_read_cspace(base, 0x3c);
    pci_reg_t class = pci_read_cspace(base, 0x8);

    struct pci_device* device = vzalloc(sizeof(struct pci_device));
    *device = (struct pci_device){ .cspace_base = base,
                                   .class_info = class,
                                   .device_info = reg1,
                                   .intr_info = intr };

    pci_probe_msi_info(device);
    pci_probe_bar_info(device);

    llist_append(&pci_devices, &device->dev_chain);

    if (!pci_bind_driver(device)) {
        kprintf(KWARN "dev.%d:%d:%d %x:%x unknown device\n",
                bus,
                dev,
                funct,
                PCI_DEV_VENDOR(reg1),
                PCI_DEV_DEVID(reg1));
    } else {
        kprintf("dev.%d:%d:%d %x:%x %s\n",
                bus,
                dev,
                funct,
                PCI_DEV_VENDOR(reg1),
                PCI_DEV_DEVID(reg1),
                device->driver.type->name);
    }
}

void
pci_probe()
{
    // 暴力扫描所有PCI设备
    // XXX: 尽管最多会有256条PCI总线，但就目前而言，只考虑bus #0就足够了
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

void
__pci_read_cspace(struct twimap* map)
{
    struct pci_device* pcidev = (struct pci_device*)(map->data);

    for (size_t i = 0; i < 256; i += sizeof(pci_reg_t)) {
        *(pci_reg_t*)(map->buffer + i) =
          pci_read_cspace(pcidev->cspace_base, i);
    }

    map->size_acc = 256;
}

void
__pci_read_revid(struct twimap* map)
{
    int class = twimap_data(map, struct pci_device*)->class_info;
    twimap_printf(map, "0x%x", PCI_DEV_REV(class));
}

void
__pci_read_class(struct twimap* map)
{
    int class = twimap_data(map, struct pci_device*)->class_info;
    twimap_printf(map, "0x%x", PCI_DEV_CLASS(class));
}

void
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

int
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

void
pci_setup_msi(struct pci_device* device, int vector)
{
    // Dest: APIC#0, Physical Destination, No redirection
    u32_t msi_addr = (__APIC_BASE_PADDR);

    // Edge trigger, Fixed delivery
    u32_t msi_data = vector;

    pci_write_cspace(
      device->cspace_base, PCI_MSI_ADDR(device->msi_loc), msi_addr);

    pci_reg_t reg1 = pci_read_cspace(device->cspace_base, device->msi_loc);
    pci_reg_t msg_ctl = reg1 >> 16;

    int offset = !!(msg_ctl & MSI_CAP_64BIT) * 4;
    pci_write_cspace(device->cspace_base,
                     PCI_MSI_DATA(device->msi_loc, offset),
                     msi_data & 0xffff);

    if ((msg_ctl & MSI_CAP_MASK)) {
        pci_write_cspace(
          device->cspace_base, PCI_MSI_MASK(device->msi_loc, offset), 0);
    }

    // manipulate the MSI_CTRL to allow device using MSI to request service.
    reg1 = (reg1 & 0xff8fffff) | 0x10000;
    pci_write_cspace(device->cspace_base, device->msi_loc, reg1);
}

struct pci_device*
pci_get_device_by_id(uint16_t vendorId, uint16_t deviceId)
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

void
pci_add_driver(const char* name,
               u32_t class,
               u32_t vendor,
               u32_t devid,
               pci_drv_init init)
{
    struct pci_driver* pci_drv = valloc(sizeof(*pci_drv));
    *pci_drv = (struct pci_driver){ .create_driver = init,
                                    .dev_info = (vendor << 16) | devid,
                                    .dev_class = class };
    if (name) {
        strncpy(pci_drv->name, name, PCI_DRV_NAME_LEN);
    }

    llist_append(&pci_drivers, &pci_drv->drivers);
}

int
pci_bind_driver(struct pci_device* pci_dev)
{
    struct pci_driver *pos, *n;
    llist_for_each(pos, n, &pci_drivers, drivers)
    {
        if (pos->dev_info) {
            if (pos->dev_info == pci_dev->device_info) {
                goto check_type;
            }
            continue;
        }
    check_type:
        if (pos->dev_class) {
            if (pos->dev_class == PCI_DEV_CLASS(pci_dev->class_info)) {
                pci_dev->driver.type = pos;
                pci_dev->driver.instance = pos->create_driver(pci_dev);
                return 1;
            }
        }
    }
    return 0;
}

void
pci_init()
{
    acpi_context* acpi = acpi_get_context();
    assert_msg(acpi, "ACPI not initialized.");
    if (acpi->mcfg.alloc_num) {
        // PCIe Enhanced Configuration Mechanism is supported.
        // TODO: support PCIe addressing mechanism
    }
    // Otherwise, fallback to use legacy PCI 3.0 method.
    pci_probe();

    pci_build_fsmapping();
}