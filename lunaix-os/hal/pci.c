#include <hal/pci.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/syslog.h>

LOG_MODULE("PCI")

static struct llist_header pci_devices;

void
pci_probe_msi_info(struct pci_device* device);

void
pci_probe_device(int bus, int dev, int funct)
{
    uint32_t base = PCI_ADDRESS(bus, dev, funct);
    pci_reg_t reg1 = pci_read_cspace(base, 0);

    // Vendor=0xffff则表示设备不存在
    if (PCI_DEV_VENDOR(reg1) == PCI_VENDOR_INVLD) {
        return;
    }

    pci_reg_t hdr_type = pci_read_cspace(base, 0xc);
    hdr_type = (hdr_type >> 16) & 0xff;

    if ((hdr_type & 0x80)) {
        hdr_type = hdr_type & ~0x80;
        // 探测多用途设备（multi-function device）
        for (int i = 1; i < 7; i++) {
            pci_probe_device(bus, dev, i);
        }
    }

    if (hdr_type != 0) {
        // XXX: 目前忽略所有桥接设备，比如PCI-PCI桥接器，或者是CardBus桥接器
        return;
    }

    pci_reg_t intr = pci_read_cspace(base, 0x3c);
    pci_reg_t class = pci_read_cspace(base, 0x8);

    struct pci_device* device = lxmalloc(sizeof(struct pci_device));
    *device = (struct pci_device){ .cspace_base = base,
                                   .class_info = class,
                                   .device_info = reg1,
                                   .intr_info = intr };

    pci_probe_msi_info(device);

    llist_append(&pci_devices, &device->dev_chain);
}

void
pci_probe()
{
    // 暴力扫描所有PCI设备
    // XXX: 尽管最多会有256条PCI总线，但就目前而言，只考虑bus #0就足够了
    for (int bus = 0; bus < 1; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            pci_probe_device(bus, dev, 0);
        }
    }
}

void
pci_probe_msi_info(struct pci_device* device)
{
    pci_reg_t status =
      pci_read_cspace(device->cspace_base, PCI_REG_STATUS_CMD) >> 16;

    if (!(status & 0x10)) {
        device->msi_loc = 0;
        return;
    }

    pci_reg_t cap_ptr = pci_read_cspace(device->cspace_base, 0x34) & 0xff;
    uint32_t cap_hdr;

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
pci_print_device()
{
    struct pci_device *pos, *n;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        kprintf(KINFO "(B%xh:D%xh:F%xh) Dev %x:%x, Class 0x%x\n",
                PCI_BUS_NUM(pos->cspace_base),
                PCI_SLOT_NUM(pos->cspace_base),
                PCI_FUNCT_NUM(pos->cspace_base),
                PCI_DEV_VENDOR(pos->device_info),
                PCI_DEV_DEVID(pos->device_info),
                PCI_DEV_CLASS(pos->class_info));

        kprintf(KINFO "\t IRQ: %d, INT#x: %d\n",
                PCI_INTR_IRQ(pos->intr_info),
                PCI_INTR_PIN(pos->intr_info));

        if (pos->msi_loc) {
            kprintf(KINFO "\t MSI supported (@%xh)\n", pos->msi_loc);
        }
    }
}

struct pci_device*
pci_get_device_by_id(uint16_t vendorId, uint16_t deviceId)
{
    uint32_t dev_info = vendorId | (deviceId << 16);
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
pci_get_device_by_class(uint32_t class)
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
pci_init()
{
    llist_init_head(&pci_devices);
    pci_probe();
}