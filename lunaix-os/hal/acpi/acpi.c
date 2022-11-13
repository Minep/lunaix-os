#include <hal/acpi/acpi.h>

#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

#include "parser/parser.h"

static acpi_context* ctx = NULL;

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

    acpi_rsdt_t* rsdt = rsdp->rsdt;

    ctx = vzalloc(sizeof(acpi_context));
    assert_msg(ctx, "Fail to create ACPI context");

    strncpy(ctx->oem_id, rsdt->header.oem_id, 6);
    ctx->oem_id[6] = '\0';

    size_t entry_n = (rsdt->header.length - sizeof(acpi_sdthdr_t)) >> 2;
    for (size_t i = 0; i < entry_n; i++) {
        acpi_sdthdr_t* sdthdr =
          (acpi_sdthdr_t*)((acpi_apic_t**)&(rsdt->entry))[i];
        switch (sdthdr->signature) {
            case ACPI_MADT_SIG:
                kprintf(KINFO "MADT: %p\n", sdthdr);
                madt_parse((acpi_madt_t*)sdthdr, ctx);
                break;
            case ACPI_FADT_SIG:
                // FADT just a plain structure, no need to parse.
                kprintf(KINFO "FADT: %p\n", sdthdr);
                ctx->fadt = *(acpi_fadt_t*)sdthdr;
                break;
            case ACPI_MCFG_SIG:
                kprintf(KINFO "MCFG: %p\n", sdthdr);
                mcfg_parse(sdthdr, ctx);
                break;
            default:
                break;
        }
    }

    kprintf(KINFO "ACPI: %s\n", ctx->oem_id);
}

acpi_context*
acpi_get_context()
{
    assert_msg(ctx, "ACPI is not initialized");
    return ctx;
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

uint8_t
acpi_gistranslate(uint8_t old_irq)
{
    if (old_irq >= 24) {
        return old_irq;
    }
    acpi_intso_t* int_override = ctx->madt.irq_exception[old_irq];
    return int_override ? (uint8_t)int_override->gsi : old_irq;
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
            u32_t sig_low = *((u32_t*)(mem_start + j));
            // u32_t sig_high = *((u32_t*)(mem_start+j) + 1);
            if (sig_low == ACPI_RSDP_SIG_L) {
                rsdp = (acpi_rsdp_t*)(mem_start + j);
                break;
            }
        }
    }
#else
    // You know what, I just search the entire 1MiB for Celestia's sake.
    uint8_t* mem_start = 0x4000;
    for (; mem_start < 0x100000; mem_start += 16) {
        u32_t sig_low = *((u32_t*)(mem_start));
        // XXX: do we need to compare this as well?
        // u32_t sig_high = *((u32_t*)(mem_start+j) + 1);
        if (sig_low == ACPI_RSDP_SIG_L) {
            rsdp = (acpi_rsdp_t*)(mem_start);
            break;
        }
    }
#endif

    return rsdp;
}