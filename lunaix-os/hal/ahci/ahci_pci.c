#include <lunaix/spike.h>

#include <hal/ahci/ahci.h>
#include <hal/pci.h>

static int
ahci_pci_bind(struct device_def* def, struct device* dev)
{
    struct pci_device* ahci_dev;
    struct pci_base_addr* bar6;
    struct ahci_driver* ahci_drv;
    msi_vector_t msiv;

    ahci_dev = PCI_DEVICE(dev);
    bar6 = pci_device_bar(ahci_dev, 5);
    assert_msg(pci_bar_mmio_space(bar6), "AHCI: BAR#6 is not MMIO.");

    pci_reg_t cmd = 0;
    pci_cmd_set_bus_master(&cmd);
    pci_cmd_set_mmio(&cmd);
    pci_cmd_set_msi(&cmd);
    pci_apply_command(ahci_dev, cmd);
    
    assert(pci_capability_msi(ahci_dev));

    msiv = isrm_msialloc(ahci_hba_isr);
    pci_setup_msi(ahci_dev, msiv);

    struct ahci_driver_param param = {
        .mmio_base = bar6->start,
        .mmio_size = bar6->size,
        .ahci_iv = msi_vect(msiv),
    };

    ahci_drv = ahci_driver_init(&param);
    pci_bind_instance(ahci_dev, ahci_drv);

    return 0;
}

static int
ahci_pci_init(struct device_def* def)
{
    return pci_bind_definition_all(pcidev_def(def));
}

static bool
ahci_pci_compat(struct pci_device_def* def, 
                struct pci_device* pcidev)
{
    return pci_device_class(pcidev) == AHCI_HBA_CLASS;
}


static struct pci_device_def ahcidef = {
    .devdef = { .class = DEVCLASS(DEVIF_PCI, DEVFN_STORAGE, DEV_SATA),
                .name = "Generic AHCI",
                .init = ahci_pci_init,
                .bind = ahci_pci_bind },
    .test_compatibility = ahci_pci_compat
};
EXPORT_PCI_DEVICE(ahci, &ahcidef, load_postboot);