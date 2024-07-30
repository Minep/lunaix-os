#include "parser.h"
#include <lunaix/mm/valloc.h>

void
madt_parse(acpi_madt_t* madt, acpi_context* toc)
{
    toc->madt.apic_addr = madt->apic_addr;

    // FUTURE: make madt.{apic,ioapic} as array or linked list.
    ptr_t ics_start = (ptr_t)madt + sizeof(acpi_madt_t);
    ptr_t ics_end = (ptr_t)madt + madt->header.length;

    // Cosidering only one IOAPIC present (max 24 pins)
    toc->madt.irq_exception =
      (acpi_intso_t**)vcalloc(24, sizeof(acpi_intso_t*));

    size_t so_idx = 0;
    while (ics_start < ics_end) {
        acpi_ics_hdr_t* entry = __acpi_ics_hdr(ics_start);
        switch (entry->type) {
            case ACPI_MADT_LAPIC:
                toc->madt.apic = __acpi_apic(entry);
                break;
            case ACPI_MADT_IOAPIC:
                toc->madt.ioapic = __acpi_ioapic(entry);
                break;
            case ACPI_MADT_INTSO: {
                acpi_intso_t* intso_tbl = __acpi_intso(entry);
                toc->madt.irq_exception[intso_tbl->source] = intso_tbl;
                break;
            }
            default:
                break;
        }

        ics_start += entry->length;
    }
}