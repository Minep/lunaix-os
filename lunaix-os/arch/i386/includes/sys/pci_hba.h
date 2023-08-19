#ifndef __LUNAIX_PCI_HBA_H
#define __LUNAIX_PCI_HBA_H

#include <hal/pci.h>
#include <lunaix/types.h>

#include "port_io.h"

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

static inline pci_reg_t
pci_read_cspace(ptr_t base, int offset)
{
    port_wrdword(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    return port_rddword(PCI_CONFIG_DATA);
}

static inline void
pci_write_cspace(ptr_t base, int offset, pci_reg_t data)
{
    port_wrdword(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    port_wrdword(PCI_CONFIG_DATA, data);
}

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

#endif /* __LUNAIX_PCI_HBA_H */
