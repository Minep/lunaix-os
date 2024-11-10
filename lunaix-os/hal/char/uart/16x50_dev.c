#include <lunaix/device.h>

#include <hal/pci.h>

#include "16x50.h"

extern_hook_load(isa16x50_create_once);
extern_hook_create(pci16650_pci_create);
extern_hook_register(pci16x50_pci_register);

static int
uart_16x50_load(struct device_def* def)
{
    isa16x50_create_once(def);
    return 0;
}

static int
uart_16x50_create(struct device_def* def, morph_t* morphed)
{
    if (morph_type_of(morphed, pci_probe_morpher)) {
        pci16650_pci_create(def, morphed);
    }

    return 0;
}

static int
uart_16x50_register(struct device_def* def)
{
    pci16x50_pci_register(def);
    
    return 0;
}

static struct device_def uart_dev = {
    def_device_class(GENERIC, CHAR, UART16550),
    def_device_name("16550 UART"),

    def_on_register(uart_16x50_register),
    def_on_load(uart_16x50_load),
    def_on_create(uart_16x50_create)
};
EXPORT_DEVICE(uart16550_pmio, &uart_dev, load_onboot);