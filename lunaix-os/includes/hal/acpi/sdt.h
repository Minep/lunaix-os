#ifndef __LUNAIX_ACPI_SDT_H
#define __LUNAIX_ACPI_SDT_H

#include <stdint.h>

#define ACPI_TABLE_PACKED   __attribute__((packed))

typedef struct acpi_sdthdr
{
    uint32_t signature;
    uint32_t length;
    // Revision
    uint8_t rev;
    uint8_t chksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t vendor_id;
    uint32_t vendor_rev;
} ACPI_TABLE_PACKED acpi_sdthdr_t;

typedef struct acpi_rsdt
{
    acpi_sdthdr_t header;
    acpi_sdthdr_t* entry;
} ACPI_TABLE_PACKED acpi_rsdt_t;

#endif /* __LUNAIX_ACPI_SDT_H */
