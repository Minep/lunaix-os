#include <hal/pci.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/syslog.h>

LOG_MODULE("PCI")

static struct llist_header pci_devices;

void
pci_probe_device(int bus, int dev, int funct)
{
    pci_reg_t reg1 = pci_read_cspace(bus, dev, funct, 0);
    uint32_t vendor = reg1 & 0xffff;
    pci_reg_t dev_id = reg1 >> 16;

    // Vendor=0xffff则表示设备不存在
    if (vendor == PCI_VENDOR_INVLD) {
        return;
    }

    pci_reg_t hdr_type = pci_read_cspace(bus, dev, funct, 3);
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

    pci_reg_t intr = pci_read_cspace(bus, dev, funct, 15);
    pci_reg_t class = pci_read_cspace(bus, dev, funct, 2) >> 8;

    struct pci_device* device = lxmalloc(sizeof(struct pci_device));
    *device = (struct pci_device){ .bus = bus,
                                   .dev = dev,
                                   .function = funct,
                                   .class_code = class,
                                   .vendor = vendor,
                                   .deviceId = dev_id,
                                   .type = hdr_type,
                                   .intr_line = intr & 0xff,
                                   .intr_pintype = (intr >> 8) & 0xff };

    // 读取设备的内存映射的寄存器的基地址
    for (int i = 0; i < 6; i++) {
        device->bars[i] = pci_read_cspace(bus, dev, funct, 4 + i);
    }

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
pci_print_device()
{
    struct pci_device *pos, *n;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        kprintf(KINFO "(B%xh:D%xh:F%xh) Dev %x:%x, Class 0x%x\n",
                pos->bus,
                pos->dev,
                pos->function,
                pos->vendor,
                pos->deviceId,
                pos->class_code);

        for (int i = 0; i < 6; i++) {
            kprintf(KINFO "\t BAR#%d: %p\n", i, pos->bars[i]);
        }
        kprintf(
          KINFO "\t IRQ: %d, INT#x: %d\n\n", pos->intr_line, pos->intr_pintype);
    }
}

struct pci_device*
pci_get_device(uint16_t vendorId, uint16_t deviceId)
{
    struct pci_device *pos, *n;
    llist_for_each(pos, n, &pci_devices, dev_chain)
    {
        if (pos->vendor == vendorId && pos->deviceId == deviceId) {
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