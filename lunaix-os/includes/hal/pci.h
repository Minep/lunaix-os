#ifndef __LUNAIX_PCI_H
#define __LUNAIX_PCI_H

#include <hal/io.h>
#include <lunaix/ds/llist.h>

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

#define PCI_TDEV 0x0
#define PCI_TPCIBRIDGE 0x1
#define PCI_TCARDBRIDGE 0x2

#define PCI_VENDOR_INVLD 0xffff

#define PCI_REG_VENDOR_DEV 0
#define PCI_REG_STATUS_CMD 0x4
#define PCI_REG_BAR(offset) (0x10 + (offset)*4)

#define PCI_DEV_VENDOR(x) ((x)&0xffff)
#define PCI_DEV_DEVID(x) ((x) >> 16)
#define PCI_INTR_IRQ(x) ((x)&0xff)
#define PCI_INTR_PIN(x) (((x)&0xff00) >> 8)
#define PCI_DEV_CLASS(x) ((x) >> 8)
#define PCI_DEV_REV(x) (((x)&0xff))
#define PCI_BUS_NUM(x) ((x >> 16) & 0xff)
#define PCI_SLOT_NUM(x) ((x >> 11) & 0x1f)
#define PCI_FUNCT_NUM(x) ((x >> 8) & 0x7)

#define PCI_ADDRESS(bus, dev, funct)                                           \
    (((bus)&0xff) << 16) | (((dev)&0xff) << 11) | (((funct)&0xff) << 8) |      \
      0x80000000

typedef unsigned int pci_reg_t;

// PCI device header format
// Ref: "PCI Local Bus Specification, Rev.3, Section 6.1"

struct pci_device
{
    struct llist_header dev_chain;
    uint32_t device_info;
    uint32_t class_info;
    uint32_t cspace_base;
    uint32_t msi_loc;
    uint16_t intr_info;
};

// PCI Configuration Space (C-Space) r/w:
//      Refer to "PCI Local Bus Specification, Rev.3, Section 3.2.2.3.2"

inline pci_reg_t
pci_read_cspace(uint32_t base, int offset)
{
    io_outl(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    return io_inl(PCI_CONFIG_DATA);
}

inline void
pci_write_cspace(uint32_t base, int offset, pci_reg_t data)
{
    io_outl(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    io_outl(PCI_CONFIG_DATA, data);
}

void
pci_probe();

void
pci_init();

void
pci_print_device();

struct pci_device* pci_get_device_by_class(uint32_t class);

struct pci_device*
pci_get_device_by_id(uint16_t vendorId, uint16_t deviceId);

#endif /* __LUNAIX_PCI_H */
