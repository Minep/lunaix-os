#ifndef __LUNAIX_PCI_H
#define __LUNAIX_PCI_H

#include <lunaix/device.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/types.h>
#include <lunaix/changeling.h>

#include "irq.h"

#define PCI_VENDOR_INVLD 0xffff

#define PCI_REG_VENDOR_DEV 0
#define PCI_REG_STATUS_CMD 0x4
#define PCI_REG_BAR(num) (0x10 + (num - 1) * 4)

#define PCI_DEV_VENDOR(x) ((x) & 0xffff)
#define PCI_DEV_DEVID(x) (((x) & 0xffff0000) >> 16)
#define PCI_INTR_IRQ(x) ((x) & 0xff)
#define PCI_INTR_PIN(x) (((x) & 0xff00) >> 8)
#define PCI_DEV_CLASS(x) ((x) >> 8)
#define PCI_DEV_REV(x) (((x) & 0xff))
#define PCI_BUS_NUM(x) (((x) >> 16) & 0xff)
#define PCI_SLOT_NUM(x) (((x) >> 11) & 0x1f)
#define PCI_FUNCT_NUM(x) (((x) >> 8) & 0x7)

#define PCI_BAR_MMIO(x) (!((x) & 0x1))
#define PCI_BAR_CACHEABLE(x) ((x) & 0x8)
#define PCI_BAR_TYPE(x) ((x) & 0x6)
#define PCI_BAR_ADDR_MM(x) ((x) & ~0xf)
#define PCI_BAR_ADDR_IO(x) ((x) & ~0x3)
#define PCI_BAR_COUNT 6

#define PCI_MSI_ADDR_LO(msi_base) ((msi_base) + 4)
#define PCI_MSI_ADDR_HI(msi_base) ((msi_base) + 8)
#define PCI_MSI_DATA(msi_base, offset) ((msi_base) + 8 + offset)
#define PCI_MSI_MASK(msi_base, offset) ((msi_base) + 0xc + offset)

#define MSI_CAP_64BIT 0x80
#define MSI_CAP_MASK 0x100
#define MSI_CAP_ENABLE 0x1

#define PCI_RCMD_DISABLE_INTR (1 << 10)
#define PCI_RCMD_FAST_B2B (1 << 9)
#define PCI_RCMD_BUS_MASTER (1 << 2)
#define PCI_RCMD_MM_ACCESS (1 << 1)
#define PCI_RCMD_IO_ACCESS 1

#define PCI_CFGADDR(pciloc) ((u32_t)(pciloc) << 8) | 0x80000000UL

#define PCILOC(bus, dev, funct)                                                \
    (((bus) & 0xff) << 8) | (((dev) & 0x1f) << 3) | ((funct) & 0x7)
#define PCILOC_BUS(loc) (((loc) >> 8) & 0xff)
#define PCILOC_DEV(loc) (((loc) >> 3) & 0x1f)
#define PCILOC_FN(loc) ((loc) & 0x7)

typedef unsigned int pci_reg_t;
typedef u16_t pciaddr_t;

// PCI device header format
// Ref: "PCI Local Bus Specification, Rev.3, Section 6.1"

#define BAR_TYPE_MMIO 0x1
#define BAR_TYPE_CACHABLE 0x2
#define PCI_DRV_NAME_LEN 32

struct pci_base_addr
{
    u32_t start;
    u32_t size;
    u32_t type;
};

struct pci_probe
{
    morph_t mobj;

    pciaddr_t loc;
    u16_t intr_info;
    u32_t device_info;
    u32_t class_info;
    u32_t cspace_base;
    u32_t msi_loc;
    struct pci_base_addr bar[6];

    struct device* bind;
    struct irq_domain* irq_domain;
};
#define pci_probe_morpher   morphable_attrs(pci_probe, mobj)

typedef bool (*pci_id_checker_t)(struct pci_probe*);

struct pci_registry
{
    struct hlist_node entries;
    struct device_def* definition;

    pci_id_checker_t check_compact;
};

bool
pci_register_driver(struct device_def* def, pci_id_checker_t checker);

/**
 * @brief 初始化PCI设备的基地址寄存器。返回由该基地址代表的，
 * 设备所使用的MMIO或I/O地址空间的，大小。
 * 参阅：PCI LB Spec. (Rev 3) Section 6.2.5.1, Implementation Note.
 *
 * @param dev The PCI device
 * @param bar_out Value in BAR
 * @param bar_num The index of BAR (starting from 1)
 * @return size_t
 */
size_t
pci_bar_sizing(struct pci_probe* probe, u32_t* bar_out, u32_t bar_num);

irq_t
pci_declare_msi_irq(irq_servant callback, struct pci_probe* probe);

int
pci_assign_msi(struct pci_probe* probe, irq_t irq, void* irq_spec);

/**
 * @brief Bind an abstract device instance to the pci device
 *
 * @param pcidev pci device
 * @param dev abstract device instance
 */
static inline void
pci_bind_instance(struct pci_probe* probe, struct device* dev)
{
    probe->bind = dev;

}

int
pci_bind_driver(struct pci_registry* reg);


static inline unsigned int
pci_device_vendor(struct pci_probe* probe)
{
    return PCI_DEV_VENDOR(probe->device_info);
}

static inline unsigned int
pci_device_devid(struct pci_probe* probe)
{
    return PCI_DEV_DEVID(probe->device_info);
}

static inline unsigned int
pci_device_class(struct pci_probe* probe)
{
    return PCI_DEV_CLASS(probe->class_info);
}

static inline struct pci_base_addr*
pci_device_bar(struct pci_probe* probe, int index)
{
    return &probe->bar[index];
}

static inline void
pci_cmd_set_mmio(pci_reg_t* cmd)
{
    *cmd |= PCI_RCMD_MM_ACCESS;
}

static inline ptr_t
pci_requester_id(struct pci_probe* probe)
{
    return probe->loc;
}

static inline void
pci_cmd_set_pmio(pci_reg_t* cmd)
{
    *cmd |= PCI_RCMD_IO_ACCESS;
}

static inline void
pci_cmd_set_msi(pci_reg_t* cmd)
{
    *cmd |= PCI_RCMD_DISABLE_INTR;
}

static inline void
pci_cmd_set_bus_master(pci_reg_t* cmd)
{
    *cmd |= PCI_RCMD_BUS_MASTER;
}

static inline void
pci_cmd_set_fast_b2b(pci_reg_t* cmd)
{
    *cmd |= PCI_RCMD_FAST_B2B;
}

static inline bool
pci_bar_mmio_space(struct pci_base_addr* bar)
{
    return (bar->type & BAR_TYPE_MMIO);
}

static inline bool
pci_capability_msi(struct pci_probe* probe)
{
    return !!probe->msi_loc;
}

static inline int
pci_intr_irq(struct pci_probe* probe)
{
    return PCI_INTR_IRQ(probe->intr_info);
}

void
pci_apply_command(struct pci_probe* probe, pci_reg_t cmd);

pci_reg_t
pci_read_cspace(ptr_t base, int offset);

void
pci_write_cspace(ptr_t base, int offset, pci_reg_t data);

#endif /* __LUNAIX_PCI_H */
