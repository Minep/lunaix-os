#ifndef __LUNAIX_ARCH_PCI_HBA_H
#define __LUNAIX_ARCH_PCI_HBA_H

#include <hal/pci.h>
#include <lunaix/types.h>

#define PCI_MSI_BASE 0

static inline pci_reg_t
pci_read_cspace(ptr_t base, int offset)
{
    return 0;
}

static inline void
pci_write_cspace(ptr_t base, int offset, pci_reg_t data)
{
    return;
}

static inline u16_t 
pci_config_msi_data(int vector) {
    return vector;
}

static inline ptr_t 
pci_get_msi_base() {
    return 0;
}

#endif /* __LUNAIX_ARCH_PCI_HBA_H */
