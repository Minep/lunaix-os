#ifndef __LUNAIX_PCI_H
#define __LUNAIX_PCI_H

#include <hal/io.h>
#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

#define PCI_TDEV 0x0
#define PCI_TPCIBRIDGE 0x1
#define PCI_TCARDBRIDGE 0x2

#define PCI_VENDOR_INVLD 0xffff

#define PCI_REG_VENDOR_DEV 0
#define PCI_REG_STATUS_CMD 0x4
#define PCI_REG_BAR(num) (0x10 + (num - 1) * 4)

#define PCI_DEV_VENDOR(x) ((x)&0xffff)
#define PCI_DEV_DEVID(x) ((x) >> 16)
#define PCI_INTR_IRQ(x) ((x)&0xff)
#define PCI_INTR_PIN(x) (((x)&0xff00) >> 8)
#define PCI_DEV_CLASS(x) ((x) >> 8)
#define PCI_DEV_REV(x) (((x)&0xff))
#define PCI_BUS_NUM(x) (((x) >> 16) & 0xff)
#define PCI_SLOT_NUM(x) (((x) >> 11) & 0x1f)
#define PCI_FUNCT_NUM(x) (((x) >> 8) & 0x7)

#define PCI_BAR_MMIO(x) (!((x)&0x1))
#define PCI_BAR_CACHEABLE(x) ((x)&0x8)
#define PCI_BAR_TYPE(x) ((x)&0x6)
#define PCI_BAR_ADDR_MM(x) ((x) & ~0xf)
#define PCI_BAR_ADDR_IO(x) ((x) & ~0x3)

#define PCI_MSI_ADDR(msi_base) ((msi_base) + 4)
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

#define PCI_ADDRESS(bus, dev, funct)                                           \
    (((bus)&0xff) << 16) | (((dev)&0xff) << 11) | (((funct)&0xff) << 8) |      \
      0x80000000

typedef unsigned int pci_reg_t;

// PCI device header format
// Ref: "PCI Local Bus Specification, Rev.3, Section 6.1"

#define BAR_TYPE_MMIO 0x1
#define BAR_TYPE_CACHABLE 0x2
#define PCI_DRV_NAME_LEN 32

struct pci_driver;

struct pci_base_addr
{
    u32_t start;
    u32_t size;
    u32_t type;
};

struct pci_device
{
    struct llist_header dev_chain;
    u32_t device_info;
    u32_t class_info;
    u32_t cspace_base;
    u32_t msi_loc;
    uint16_t intr_info;
    struct
    {
        struct pci_driver* type;
        void* instance;
    } driver;
    struct pci_base_addr bar[6];
};

typedef void* (*pci_drv_init)(struct pci_device*);

struct pci_driver
{
    struct llist_header drivers;
    u32_t dev_info;
    u32_t dev_class;
    pci_drv_init create_driver;
    char name[PCI_DRV_NAME_LEN];
};

// PCI Configuration Space (C-Space) r/w:
//      Refer to "PCI Local Bus Specification, Rev.3, Section 3.2.2.3.2"

static inline pci_reg_t
pci_read_cspace(u32_t base, int offset)
{
    io_outl(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    return io_inl(PCI_CONFIG_DATA);
}

static inline void
pci_write_cspace(u32_t base, int offset, pci_reg_t data)
{
    io_outl(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    io_outl(PCI_CONFIG_DATA, data);
}

/**
 * @brief 初始化PCI。这主要是通过扫描PCI总线进行拓扑重建。注意，该
 * 初始化不包括针对每个设备的初始化，因为那是设备驱动的事情。
 *
 */
void
pci_init();

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
pci_get_device_by_id(uint16_t vendorId, uint16_t deviceId);

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
 * @brief 配置并启用设备MSI支持。
 * 参阅：PCI LB Spec. (Rev 3) Section 6.8 & 6.8.1
 * 以及：Intel Manual, Vol 3, Section 10.11
 *
 * @param device PCI device
 * @param vector interrupt vector.
 */
void
pci_setup_msi(struct pci_device* device, int vector);

void
pci_add_driver(const char* name,
               u32_t class,
               u32_t vendor,
               u32_t devid,
               pci_drv_init init);

int
pci_bind_driver(struct pci_device* pci_dev);

#endif /* __LUNAIX_PCI_H */
