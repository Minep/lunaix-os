#include <hal/acpi/acpi.h>

#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

#include "parser/parser.h"

static acpi_context* ctx = NULL;

LOG_MODULE("ACPI")

int
acpi_rsdp_validate(acpi_rsdp_t* rsdp);

acpi_context*
acpi_get_context()
{
    assert_msg(ctx, "ACPI is not initialized");
    return ctx;
}

int
acpi_rsdp_validate(acpi_rsdp_t* rsdp)
{
    u8_t sum = 0;
    u8_t* rsdp_ptr = (u8_t*)rsdp;
    for (size_t i = 0; i < 20; i++) {
        sum += *(rsdp_ptr + i);
    }

    return sum == 0;
}

u8_t
acpi_gsimap(u8_t old_irq)
{
    if (old_irq >= 24) {
        return old_irq;
    }
    acpi_intso_t* int_override = ctx->madt.irq_exception[old_irq];
    return int_override ? (u8_t)int_override->gsi : old_irq;
}

#define VIRTUAL_BOX_PROBLEM

acpi_rsdp_t*
acpi_locate_rsdp()
{
    acpi_rsdp_t* rsdp = NULL;

    ptr_t mem_start = 0x4000;
    for (; mem_start < 0x100000; mem_start += 16) {
        u32_t sig_low = *((u32_t*)mem_start);
        // XXX: do we need to compare this as well?
        // u32_t sig_high = *((u32_t*)(mem_start+j) + 1);

        if (sig_low == ACPI_RSDP_SIG_L) {
            rsdp = (acpi_rsdp_t*)mem_start;
            break;
        }
    }

    return rsdp;
}

static int
acpi_init(struct device_def* devdef)
{
    acpi_rsdp_t* rsdp = acpi_locate_rsdp();

    assert_msg(rsdp, "Fail to locate ACPI_RSDP");
    assert_msg(acpi_rsdp_validate(rsdp), "Invalid ACPI_RSDP (checksum failed)");

    acpi_rsdt_t* rsdt = rsdp->rsdt;

    ctx = vzalloc(sizeof(acpi_context));
    assert_msg(ctx, "Fail to create ACPI context");

    strncpy(ctx->oem_id, rsdt->header.oem_id, 6);
    ctx->oem_id[6] = '\0';

    size_t entry_n = (rsdt->header.length - sizeof(acpi_sdthdr_t)) >> 2;
    for (size_t i = 0; i < entry_n; i++) {
        acpi_sdthdr_t* sdthdr = __acpi_sdthdr(rsdt->entry[i]);
        switch (sdthdr->signature) {
            case ACPI_MADT_SIG:
                madt_parse(__acpi_madt(sdthdr), ctx);
                break;
            case ACPI_FADT_SIG:
                // FADT just a plain structure, no need to parse.
                ctx->fadt = *__acpi_fadt(sdthdr);
                break;
            case ACPI_MCFG_SIG:
                mcfg_parse(sdthdr, ctx);
                break;
            default:
                break;
        }
    }

    return 0;
}

struct device_def acpi_sysdev = { .name = "ACPI Proxy",
                                  .class =
                                    DEVCLASS(DEVIF_FMW, DEVFN_CFG, DEV_ACPI),
                                  .init = acpi_init };
EXPORT_DEVICE(acpi, &acpi_sysdev, load_sysconf);