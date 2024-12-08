#include <lunaix/device.h>
#include <asm-generic/isrm.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/mmio.h>

#include <hal/pci.h>

#include "16x50.h"

#define PCI_DEVICE_16x50_UART    0x070000

LOG_MODULE("16x50-pci")

static DEFINE_LLIST(pci_ports);


static void
uart_msi_irq_handler(irq_t irq, const struct hart_state* hstate)
{
    struct uart16550* uart;
    
    uart = irq_payload(irq, struct uart16550);

    assert(uart);
    uart_handle_irq(irq, uart);
}

static bool
pci16x50_check_compat(struct pci_probe* probe)
{
    unsigned int classid;
    classid = pci_device_class(probe);

    return (classid & 0xffff00) == PCI_DEVICE_16x50_UART;
}

int
pci16x50_pci_register(struct device_def* def)
{
    return !pci_register_driver(def, pci16x50_check_compat);
}

int
pci16x50_pci_create(struct device_def* def, morph_t* obj)
{
    struct pci_base_addr* bar;
    struct pci_probe* probe;
    struct uart16550* uart;
    struct serial_dev* sdev;
    irq_t irq;
    
    probe = changeling_reveal(obj, pci_probe_morpher);

    pci_reg_t cmd = 0;

    for (int i = 0; i < PCI_BAR_COUNT; i++) 
    {
        cmd = 0;
        pci_cmd_set_msi(&cmd);
        
        bar = pci_device_bar(probe, i);
        if (bar->size == 0) {
            continue;
        }

        if (!pci_bar_mmio_space(bar)) {
#ifdef CONFIG_PCI_PMIO
            pci_cmd_set_pmio(&cmd);
            pci_apply_command(probe, cmd);

            uart = uart16x50_pmio_create(bar->start);
#else
            WARN("plaform configured to not support pmio access.");
            continue;
#endif
        } else 
        {
            pci_cmd_set_mmio(&cmd);
            pci_apply_command(probe, cmd);

            uart = uart16x50_mmio_create(bar->start, bar->size);
        }

        if (!uart) {
            WARN("not accessible (BAR #%d)", i);
            continue;
        }

        if (!pci_capability_msi(probe)) {
            WARN("failed to fallback to legacy INTx: not supported.");
            continue;
        }

        sdev = uart_create_serial(uart, &def->class, &pci_ports, "PCI");

        irq = pci_declare_msi_irq(uart_msi_irq_handler, probe, NULL);
        irq_set_payload(irq, uart);
        pci_assign_msi(probe, irq);

        INFO("base: 0x%x (%s), %s", 
                bar->start, 
                pci_bar_mmio_space(bar) ? "mmio" : "pmio",
                pci_capability_msi(probe) ? "msi" : "intx, re-routed");
        
        uart->irq = irq;

        pci_bind_instance(probe, sdev->dev);
    }

    return 0;
}
