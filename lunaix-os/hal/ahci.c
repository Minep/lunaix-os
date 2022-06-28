/**
 * @file ahci.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief A software implementation of Serial ATA AHCI 1.3.1 Specification
 * @version 0.1
 * @date 2022-06-28
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <hal/ahci.h>
#include <hal/pci.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("AHCI")

static struct ahci_hba hba;

void
ahci_init()
{
    struct pci_device* ahci_dev = pci_get_device_by_class(0x10601);
    assert_msg(ahci_dev, "AHCI: Not found.");

    uintptr_t bar6, size;
    size = pci_bar_sizing(ahci_dev, &bar6, 6);
    assert_msg(bar6 && PCI_BAR_MMIO(bar6), "AHCI: BAR#6 is not MMIO.");

    hba.base = (hba_reg_t*)ioremap(PCI_BAR_ADDR_MM(bar6), size);

    // Enable AHCI, Enable interrupt generation.
    hba.base[HBA_RGHC] |= 0x80000002;

    // As per section 3.1.1, this is 0 based value.
    hba.ports_num = (hba.base[HBA_RCAP] & 0x1f) + 1;
    hba.version = hba.base[HBA_RVER];

    kprintf(KINFO "Version: %x; Ports: %d\n", hba.version, hba.ports_num);

    hba_reg_t pmap = hba.base[HBA_RPI];
    for (size_t i = 0; i < 32; i++, pmap = pmap >> 1) {
        if (!(pmap & 0x1)) {
            continue;
        }
        hba.ports[i] = (hba_reg_t*)(&hba.base[HBA_RPBASE] + i);
        kprintf("\t Port#%d, clb: %p, fis: %p\n",
                i + 1,
                hba.ports[i][HBA_RPxCLB],
                hba.ports[i][HBA_RPxFB]);

        // TODO: (?) Rebasing each port's command list and FIS
    }
}