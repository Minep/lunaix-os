#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <hal/ahci/ahci.h>
#include <hal/pci.h>

static int
ahci_pci_create(struct device_def* def, morph_t* morphed)
{
    struct pci_probe* probe;
    struct device* dev;
    struct pci_base_addr* bar6;
    struct ahci_driver* ahci_drv;
    msi_vector_t msiv;

    probe = changeling_try_reveal(morphed, pci_probe_morpher);
    if (!probe) {
        return EINVAL;
    }

    bar6 = pci_device_bar(probe, 5);
    assert_msg(pci_bar_mmio_space(bar6), "AHCI: BAR#6 is not MMIO.");

    pci_reg_t cmd = 0;
    pci_cmd_set_bus_master(&cmd);
    pci_cmd_set_mmio(&cmd);
    pci_cmd_set_msi(&cmd);
    pci_apply_command(probe, cmd);
    
    assert(pci_capability_msi(probe));

    msiv = pci_msi_setup_simple(probe, ahci_hba_isr);

    struct ahci_driver_param param = {
        .mmio_base = bar6->start,
        .mmio_size = bar6->size,
        .ahci_iv = msi_vect(msiv),
    };

    ahci_drv = ahci_driver_init(&param);
    dev = device_allocvol(NULL, ahci_drv);

    device_setname(dev_meta(dev), 
                   "pci-ahci%d", devclass_mkvar(&def->class));

    pci_bind_instance(probe, dev);

    return 0;
}

static bool
ahci_pci_compat(struct pci_probe* probe)
{
    return pci_device_class(probe) == AHCI_HBA_CLASS;
}

static int
ahci_pci_register(struct device_def* def)
{
    return !pci_register_driver(def, ahci_pci_compat);
}


static struct device_def ahcidef = 
{
    def_device_class(PCI, STORAGE, SATA),
    def_device_name("Generic AHCI (pci-bus)"),

    def_on_register(ahci_pci_register),
    def_on_create(ahci_pci_create),

    def_non_trivial
};
EXPORT_DEVICE(ahci, &ahcidef, load_postboot);