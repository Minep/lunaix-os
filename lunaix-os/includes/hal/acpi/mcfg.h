#ifndef __LUNAIX_MCFG_H
#define __LUNAIX_MCFG_H

#include "sdt.h"

struct acpi_mcfg_alloc
{
    u32_t base_addr_lo;
    u32_t base_addr_hi;
    u16_t pci_seg_num;
    u8_t pci_bus_start;
    u8_t pci_bus_end;
    u32_t reserve;
} ACPI_TABLE_PACKED;

struct mcfg_alloc_info
{
    u32_t base_addr;
    u16_t pci_seg_num;
    u8_t pci_bus_start;
    u8_t pci_bus_end;
};

struct acpi_mcfg_toc
{
    size_t alloc_num;
    struct mcfg_alloc_info* allocations;
};

#endif /* __LUNAIX_MCFG_H */
