#include <lunaix/device.h>
#include <asm/isrm.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/mmio.h>

#include <hal/pci.h>

#include "16x50.h"

#define PCI_DEVICE_16x50_UART    0x070000

LOG_MODULE("16x50-pci")

static DEFINE_LLIST(pci_ports);

static void
uart_msi_irq_handler(const struct hart_state* hstate)
{
    int vector;
    struct uart16550* uart;
    
    vector = hart_vector_stamp(hstate);
    uart = (struct uart16550*)isrm_get_payload(hstate);

    assert(uart);
    uart_handle_irq(vector, uart);
}

static void
uart_intx_irq_handler(const struct hart_state* hstate)
{
    int vector = hart_vector_stamp(hstate);
    uart_handle_irq_overlap(vector, &pci_ports);
}

static int
pci16550_init(struct device_def* def)
{
    return pci_bind_definition_all(pcidev_def(def));
}

static bool
pci16650_check_compat(struct pci_device_def* def, 
                      struct pci_device* pcidev)
{
    unsigned int classid;
    classid = pci_device_class(pcidev);

    return (classid & 0xffff00) == PCI_DEVICE_16x50_UART;
}

static int
pci16650_binder(struct device_def* def, struct device* dev)
{
    int irq;
    struct pci_base_addr* bar;
    struct pci_device* pcidev;
    struct uart16550* uart;
    struct serial_dev* sdev;
    
    pcidev = PCI_DEVICE(dev);

    pci_reg_t cmd = 0;

    for (int i = 0; i < PCI_BAR_COUNT; i++) 
    {
        cmd = 0;
        pci_cmd_set_msi(&cmd);
        
        bar = pci_device_bar(pcidev, i);
        if (bar->size == 0) {
            continue;
        }
                
        if (!pci_bar_mmio_space(bar)) {
            pci_cmd_set_pmio(&cmd);
            pci_apply_command(pcidev, cmd);

            uart = uart16x50_pmio_create(bar->start);
        }
        else {
            pci_cmd_set_mmio(&cmd);
            pci_apply_command(pcidev, cmd);

            uart = uart16x50_mmio_create(bar->start, bar->size);
        }

        if (!uart) {
            WARN("not accessible (BAR #%d)", i);
            continue;
        }

        if (pci_capability_msi(pcidev)) {
            irq = isrm_ivexalloc(uart_msi_irq_handler);
            isrm_set_payload(irq, __ptr(uart));
            pci_setup_msi(pcidev, irq);
        }
        else {
            irq = pci_intr_irq(pcidev);
            irq = isrm_bindirq(irq, uart_intx_irq_handler);
        }

        INFO("base: 0x%x (%s), irq=%d (%s)", 
                bar->start, 
                pci_bar_mmio_space(bar) ? "mmio" : "pmio",
                irq, 
                pci_capability_msi(pcidev) ? "msi" : "intx, re-routed");
        
        uart->iv = irq;

        sdev = uart_create_serial(uart, &def->class, &pci_ports, "PCI");
        pci_bind_instance(pcidev, sdev);
    }

    return 0;
}

static struct pci_device_def uart_pci_def = {
    .devdef = { .class = DEVCLASS(DEVIF_PCI, DEVFN_CHAR, DEV_UART16550),
                .name = "16550 UART (PCI/MMIO)",
                .init = pci16550_init,
                .bind = pci16650_binder },
    .test_compatibility = pci16650_check_compat
};
EXPORT_PCI_DEVICE(uart16550_pci, &uart_pci_def, load_onboot);