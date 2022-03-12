#ifndef __LUNAIX_ACPI_ACPI_H
#define __LUNAIX_ACPI_ACPI_H

#include <stdint.h>
#include <stddef.h>
#include <arch/x86/boot/multiboot.h>

#include "sdt.h"
#include "madt.h"

#define ACPI_RSDP_SIG_L       0x20445352      // 'RSD '
#define ACPI_RSDP_SIG_H      0x20525450      // 'PTR '

#define ACPI_MADT_SIG        0x43495041      // 'APIC'

typedef struct {
    uint32_t signature_l;
    uint32_t signature_h;
    uint8_t chksum;
    uint8_t oem_id[6];
    // Revision
    uint8_t rev;
    acpi_rsdt_t* rsdt;
    uint32_t length;
    acpi_sdthdr_t* xsdt;
    uint8_t x_chksum;
    char reserved[3];    // Reserved field
} __attribute__((packed)) acpi_rsdp_t;

/**
 * @brief Main TOC of ACPI tables, provide hassle-free access of ACPI info.
 * 
 */
typedef struct
{
    // Make it as null terminated
    char oem_id[7];
    acpi_madt_toc_t madt;
} acpi_context;

int
acpi_init(multiboot_info_t* mb_info);

acpi_context*
acpi_get_context();

#endif /* __LUNAIX_ACPI_ACPI_H */
