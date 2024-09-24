#include <hal/pci.h>
#include <asm/x86_pmio.h>

#ifdef CONFIG_PCI_PMIO
#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

pci_reg_t
pci_read_cspace(ptr_t base, int offset)
{
    port_wrdword(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    return port_rddword(PCI_CONFIG_DATA);
}

void
pci_write_cspace(ptr_t base, int offset, pci_reg_t data)
{
    port_wrdword(PCI_CONFIG_ADDR, base | (offset & ~0x3));
    port_wrdword(PCI_CONFIG_DATA, data);
}

#endif

u16_t
pci_config_msi_data(int vector) {
    return vector;
}

ptr_t
pci_get_msi_base() {
    return 0xFEE00000;
}

