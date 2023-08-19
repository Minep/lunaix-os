#include <sys/apic.h>
#include <sys/pci_hba.h>

void
pci_setup_msi(struct pci_device* device, int vector)
{
    // Dest: APIC#0, Physical Destination, No redirection
    u32_t msi_addr = (__APIC_BASE_PADDR);

    // Edge trigger, Fixed delivery
    u32_t msi_data = vector;

    pci_write_cspace(
      device->cspace_base, PCI_MSI_ADDR(device->msi_loc), msi_addr);

    pci_reg_t reg1 = pci_read_cspace(device->cspace_base, device->msi_loc);
    pci_reg_t msg_ctl = reg1 >> 16;

    int offset = !!(msg_ctl & MSI_CAP_64BIT) * 4;
    pci_write_cspace(device->cspace_base,
                     PCI_MSI_DATA(device->msi_loc, offset),
                     msi_data & 0xffff);

    if ((msg_ctl & MSI_CAP_MASK)) {
        pci_write_cspace(
          device->cspace_base, PCI_MSI_MASK(device->msi_loc, offset), 0);
    }

    // manipulate the MSI_CTRL to allow device using MSI to request service.
    reg1 = (reg1 & 0xff8fffff) | 0x10000;
    pci_write_cspace(device->cspace_base, device->msi_loc, reg1);
}