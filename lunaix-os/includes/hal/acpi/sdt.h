#ifndef __LUNAIX_ACPI_SDT_H
#define __LUNAIX_ACPI_SDT_H

#include <lunaix/types.h>

#define ACPI_TABLE_PACKED __attribute__((packed))

typedef struct acpi_sdthdr
{
    u32_t signature;
    u32_t length;
    // Revision
    uint8_t rev;
    uint8_t chksum;
    char oem_id[6];
    char oem_table_id[8];
    u32_t oem_rev;
    u32_t vendor_id;
    u32_t vendor_rev;
} ACPI_TABLE_PACKED acpi_sdthdr_t;

typedef struct acpi_rsdt
{
    acpi_sdthdr_t header;
    acpi_sdthdr_t* entry;
} ACPI_TABLE_PACKED acpi_rsdt_t;

#endif /* __LUNAIX_ACPI_SDT_H */
