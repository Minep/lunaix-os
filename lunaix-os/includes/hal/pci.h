#ifndef __LUNAIX_PCI_H
#define __LUNAIX_PCI_H

#include <lunaix/device.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

#define EXPORT_PCI_DEVICE(id, pci_devdef, stage)                               \
    EXPORT_DEVICE(id, &(pci_devdef)->devdef, stage)

#define PCI_MATCH_EXACT -1
#define PCI_MATCH_ANY 0
#define PCI_MATCH_VENDOR 0xffff

#define PCI_TDEV 0x0
#define PCI_TPCIBRIDGE 0x1
#define PCI_TCARDBRIDGE 0x2

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

#define PCI_ID_ANY (-1)

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

struct pci_device
{
    struct device dev;
    struct llist_header dev_chain;
    struct hlist_node dev_cache;

    struct
    {
        struct device* dev;
        struct device_def* def;
    } binding;

    pciaddr_t loc;
    u16_t intr_info;
    u32_t device_info;
    u32_t class_info;
    u32_t cspace_base;
    u32_t msi_loc;
    struct pci_base_addr bar[6];
};
#define PCI_DEVICE(devbase) (container_of((devbase), struct pci_device, dev))

struct pci_device_list
{
    struct llist_header peers;
    struct pci_device* pcidev;
};

typedef void* (*pci_drv_init)(struct pci_device*);

#define PCI_DEVIDENT(vendor, id)                                               \
    ((((id) & 0xffff) << 16) | (((vendor) & 0xffff)))

struct pci_device_def
{
    struct device_def devdef;

    bool (*test_compatibility)(struct pci_device_def*, struct pci_device*);
};
#define pcidev_def(dev_def_ptr)                                                \
    container_of((dev_def_ptr), struct pci_device_def, devdef)

#define binded_pcidev(pcidev) ((pcidev)->binding.def)

/**
 * @brief 根据类型代码（Class Code）去在拓扑中寻找一个设备
 * 类型代码请参阅： PCI LB Spec. Appendix D.
 *
 * @return struct pci_device*
 */
struct pci_device* pci_get_device_by_class(u32_t class);

/**
 * @brief 根据设备商ID和设备ID，在拓扑中寻找一个设备
 *
 * @param vendorId
 * @param deviceId
 * @return struct pci_device*
 */
struct pci_device*
pci_get_device_by_id(u16_t vendorId, u16_t deviceId);

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
pci_bar_sizing(struct pci_device* dev, u32_t* bar_out, u32_t bar_num);

/**
 * @brief Bind an abstract device instance to the pci device
 *
 * @param pcidev pci device
 * @param devobj abstract device instance
 */
void
pci_bind_instance(struct pci_device* pcidev, void* devobj);

void
pci_probe_bar_info(struct pci_device* device);

void
pci_setup_msi(struct pci_device* device, int vector);

void
pci_probe_msi_info(struct pci_device* device);

int
pci_bind_definition(struct pci_device_def* pcidev_def, bool* more);

int
pci_bind_definition_all(struct pci_device_def* pcidef);

static inline unsigned int
pci_device_vendor(struct pci_device* pcidev)
{
    return PCI_DEV_VENDOR(pcidev->device_info);
}

static inline unsigned int
pci_device_devid(struct pci_device* pcidev)
{
    return PCI_DEV_DEVID(pcidev->device_info);
}

static inline unsigned int
pci_device_class(struct pci_device* pcidev)
{
    return PCI_DEV_CLASS(pcidev->class_info);
}

static inline struct pci_base_addr*
pci_device_bar(struct pci_device* pcidev, int index)
{
    return &pcidev->bar[index];
}

static inline void
pci_cmd_set_mmio(pci_reg_t* cmd)
{
    *cmd |= PCI_RCMD_MM_ACCESS;
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
pci_capability_msi(struct pci_device* pcidev)
{
    return !!pcidev->msi_loc;
}

static inline int
pci_intr_irq(struct pci_device* pcidev)
{
    return PCI_INTR_IRQ(pcidev->intr_info);
}

void
pci_apply_command(struct pci_device* pcidev, pci_reg_t cmd);

pci_reg_t
pci_read_cspace(ptr_t base, int offset);

void
pci_write_cspace(ptr_t base, int offset, pci_reg_t data);

u16_t
pci_config_msi_data(int vector);

ptr_t
pci_get_msi_base();


#endif /* __LUNAIX_PCI_H */
