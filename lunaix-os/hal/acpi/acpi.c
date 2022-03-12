#include <hal/acpi/acpi.h>

#include <lunaix/mm/kalloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

#include "parser/madt_parser.h"

acpi_context* toc = NULL;

LOG_MODULE("ACPI")

int
acpi_rsdp_validate(acpi_rsdp_t* rsdp);

acpi_rsdp_t*
acpi_locate_rsdp(multiboot_info_t* mb_info);

int
acpi_init(multiboot_info_t* mb_info)
{
    acpi_rsdp_t* rsdp = acpi_locate_rsdp(mb_info);

    assert_msg(rsdp, "Fail to locate ACPI_RSDP");
    assert_msg(acpi_rsdp_validate(rsdp), "Invalid ACPI_RSDP (checksum failed)");

    kprintf(KINFO "RSDP found at %p, RSDT: %p\n", rsdp, rsdp->rsdt);

    acpi_rsdt_t* rsdt = rsdp->rsdt;

    toc = lxcalloc(1, sizeof(acpi_context));
    assert_msg(toc, "Fail to create ACPI context");

    strncpy(toc->oem_id, rsdt->header.oem_id, 6);
    toc->oem_id[6] = '\0';

    size_t entry_n = (rsdt->header.length - sizeof(acpi_sdthdr_t)) >> 2;
    for (size_t i = 0; i < entry_n; i++) {
        acpi_sdthdr_t* sdthdr = ((acpi_apic_t**)&(rsdt->entry))[i];
        switch (sdthdr->signature) {
            case ACPI_MADT_SIG:
                madt_parse((acpi_madt_t*)sdthdr, toc);
                break;
            default:
                break;
        }
    }

    kprintf(KINFO "OEM: %s\n", toc->oem_id);
    kprintf(KINFO "IOAPIC address: %p\n", toc->madt.ioapic->ioapic_addr);
    kprintf(KINFO "APIC address: %p\n", toc->madt.apic_addr);
    
    for (size_t i = 0; i < 24; i++) {
        acpi_intso_t* intso = toc->madt.irq_exception[i];
        if (!intso)
            continue;

        kprintf(KINFO "IRQ #%u -> GSI #%u\n", intso->source, intso->gsi);
    }
}

acpi_context*
acpi_get_context()
{
    assert_msg(toc, "ACPI is not initialized");
    return toc;
}

int
acpi_rsdp_validate(acpi_rsdp_t* rsdp)
{
    uint8_t sum = 0;
    uint8_t* rsdp_ptr = (uint8_t*)rsdp;
    for (size_t i = 0; i < 20; i++) {
        sum += *(rsdp_ptr + i);
    }

    return sum == 0;
}

#define VIRTUAL_BOX_PROBLEM

acpi_rsdp_t*
acpi_locate_rsdp(multiboot_info_t* mb_info)
{
    acpi_rsdp_t* rsdp = NULL;

    // You can't trust memory map from multiboot in virtual box!
    // They put ACPI RSDP in the FUCKING 0xe0000 !!!
    // Which is reported to be free area bt multiboot!
    // SWEET CELESTIA!!!
#ifndef VIRTUAL_BOX_PROBLEM
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mb_info->mmap_addr;
    for (size_t i = 0, j = 0; j < mb_info->mmap_length && !rsdp;
         i++, j += MB_MMAP_ENTRY_SIZE) {
        multiboot_memory_map_t entry = mmap[i];
        if (entry.type != MULTIBOOT_MEMORY_RESERVED ||
            entry.addr_low > 0x100000) {
            continue;
        }

        uint8_t* mem_start = entry.addr_low & ~0xf;
        size_t len = entry.len_low;
        for (size_t j = 0; j < len; j += 16) {
            uint32_t sig_low = *((uint32_t*)(mem_start + j));
            // uint32_t sig_high = *((uint32_t*)(mem_start+j) + 1);
            if (sig_low == ACPI_RSDP_SIG_L) {
                rsdp = (acpi_rsdp_t*)(mem_start + j);
                break;
            }
        }
    }
#else
    // You know what, I just search the entire 1MiB for Celestia's sake.
    uint8_t* mem_start = 0x4000;
    for (size_t j = 0; j < 0x100000; j += 16) {
        uint32_t sig_low = *((uint32_t*)(mem_start + j));
        // uint32_t sig_high = *((uint32_t*)(mem_start+j) + 1);
        if (sig_low == ACPI_RSDP_SIG_L) {
            rsdp = (acpi_rsdp_t*)(mem_start + j);
            break;
        }
    }
#endif

    return rsdp;
}