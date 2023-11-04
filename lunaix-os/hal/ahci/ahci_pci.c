#include <lunaix/spike.h>

#include <hal/ahci/ahci.h>
#include <hal/pci.h>
#include <sys/pci_hba.h>

static int
ahci_pci_bind(struct device_def* def, struct device* dev)
{
    struct pci_device* ahci_dev = container_of(dev, struct pci_device, dev);

    struct pci_base_addr* bar6 = &ahci_dev->bar[5];
    assert_msg(bar6->type & BAR_TYPE_MMIO, "AHCI: BAR#6 is not MMIO.");

    pci_reg_t cmd = pci_read_cspace(ahci_dev->cspace_base, PCI_REG_STATUS_CMD);

    // 禁用传统中断（因为我们使用MSI），启用MMIO访问，允许PCI设备间访问
    cmd |= (PCI_RCMD_MM_ACCESS | PCI_RCMD_DISABLE_INTR | PCI_RCMD_BUS_MASTER);

    pci_write_cspace(ahci_dev->cspace_base, PCI_REG_STATUS_CMD, cmd);

    int iv = isrm_ivexalloc(ahci_hba_isr);
    pci_setup_msi(ahci_dev, iv);

    struct ahci_driver_param param = {
        .mmio_base = bar6->start,
        .mmio_size = bar6->size,
        .ahci_iv = iv,
    };

    struct ahci_driver* ahci_drv = ahci_driver_init(&param);
    pci_bind_instance(ahci_dev, ahci_drv);

    return 0;
}

static int
ahci_pci_init(struct device_def* def)
{
    return pci_bind_definition_all(pcidev_def(def));
}

static struct pci_device_def ahcidef = {
    .dev_class = AHCI_HBA_CLASS,
    .ident_mask = PCI_MATCH_ANY,
    .devdef = { .class = DEVCLASS(DEVIF_PCI, DEVFN_STORAGE, DEV_SATA),
                .name = "Generic SATA",
                .init = ahci_pci_init,
                .bind = ahci_pci_bind }
};
EXPORT_PCI_DEVICE(ahci, &ahcidef, load_postboot);