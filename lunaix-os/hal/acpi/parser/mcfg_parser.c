#include "lunaix/syslog.h"
#include "parser.h"
#include <lunaix/mm/valloc.h>

LOG_MODULE("MCFG")

void
mcfg_parse(acpi_sdthdr_t* mcfg, acpi_context* toc)
{
    size_t alloc_num = (mcfg->length - sizeof(acpi_sdthdr_t) - 8) /
                       sizeof(struct acpi_mcfg_alloc);
    struct acpi_mcfg_alloc* allocs =
      (struct acpi_mcfg_alloc*)((uintptr_t)mcfg + (sizeof(acpi_sdthdr_t) + 8));

    toc->mcfg.alloc_num = alloc_num;
    toc->mcfg.allocations = valloc(sizeof(struct mcfg_alloc_info) * alloc_num);

    for (size_t i = 0; i < alloc_num; i++) {
        toc->mcfg.allocations[i] = (struct mcfg_alloc_info){
            .base_addr = allocs[i].base_addr_lo,
            .pci_bus_start = allocs[i].pci_bus_start,
            .pci_bus_end = allocs[i].pci_bus_end,
            .pci_seg_num = allocs[i].pci_seg_num,
        };
    }
}