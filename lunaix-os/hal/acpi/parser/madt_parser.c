#include "parser.h"
#include <lunaix/mm/valloc.h>

void
madt_parse(acpi_madt_t* madt, acpi_context* toc)
{
    toc->madt.apic_addr = madt->apic_addr;

    // FUTURE: make madt.{apic,ioapic} as array or linked list.
    uint8_t* ics_start = (uint8_t*)((uintptr_t)madt + sizeof(acpi_madt_t));
    uintptr_t ics_end = (uintptr_t)madt + madt->header.length;

    // Cosidering only one IOAPIC present (max 24 pins)
    toc->madt.irq_exception =
      (acpi_intso_t**)vcalloc(24, sizeof(acpi_intso_t*));

    size_t so_idx = 0;
    while (ics_start < ics_end) {
        acpi_ics_hdr_t* entry = (acpi_ics_hdr_t*)ics_start;
        switch (entry->type) {
            case ACPI_MADT_LAPIC:
                toc->madt.apic = (acpi_apic_t*)entry;
                break;
            case ACPI_MADT_IOAPIC:
                toc->madt.ioapic = (acpi_ioapic_t*)entry;
                break;
            case ACPI_MADT_INTSO: {
                acpi_intso_t* intso_tbl = (acpi_intso_t*)entry;
                toc->madt.irq_exception[intso_tbl->source] = intso_tbl;
                break;
            }
            default:
                break;
        }

        ics_start += entry->length;
    }
}