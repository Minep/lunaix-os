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

#define PCI_REG_VENDER 0x0
#define PCI_REG_DEV 0x1
#define PCI_REG_HDRTYPE 0x7

#define PCI_ADDRESS(bus, dev, funct, reg)                                      \
    (((bus)&0xff) << 16) | (((dev)&0xff) << 11) | (((funct)&0xff) << 8) |      \
      (((reg)&0xff) << 2) | 0x80000000

typedef unsigned int pci_reg_t;

// PCI device header format
// Ref: "PCI Local Bus Specification, Rev.3, Section 6.1"

struct pci_device
{
    struct llist_header dev_chain;
    uint16_t vendor;
    uint16_t deviceId;
    uint32_t class_code;
    uint8_t bus;
    uint8_t dev;
    uint8_t function;
    uint8_t type;
    uint8_t intr_line;
    uint8_t intr_pintype;
    uint32_t bars[6];
};

// PCI Configuration Space (C-Space) r/w:
//      Refer to "PCI Local Bus Specification, Rev.3, Section 3.2.2.3.2"

inline pci_reg_t
pci_read_cspace(int bus, int dev, int funct, int reg)
{
    io_outl(PCI_CONFIG_ADDR, PCI_ADDRESS(bus, dev, funct, reg));
    return io_inl(PCI_CONFIG_DATA);
}

inline void
pci_write_cspace(int bus, int dev, int funct, int reg, pci_reg_t data)
{
    io_outl(PCI_CONFIG_ADDR, PCI_ADDRESS(bus, dev, funct, reg));
    io_outl(PCI_CONFIG_DATA, data);
}

void
pci_probe();

void
pci_init();

void
pci_print_device();

struct pci_device*
pci_get_device(uint16_t vendorId, uint16_t deviceId);

#endif /* __LUNAIX_PCI_H */
