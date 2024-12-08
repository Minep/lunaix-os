/**
 * @file apic.c
 * @author Lunaixsky
 * @brief Abstraction for Advanced Programmable Interrupts Controller (APIC)
 * @version 0.1
 * @date 2022-03-06
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "asm/x86.h"
#include "asm/x86_cpu.h"
#include "asm/soc/apic.h"

#include <asm/hart.h>
#include <asm/x86_isrm.h>

#include <lunaix/mm/mmio.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/device.h>

#include <hal/irq.h>
#include <hal/acpi/acpi.h>

#include "pic.h"

LOG_MODULE("APIC")

#define IOAPIC_IOREGSEL 0x00
#define IOAPIC_IOWIN 0x10
#define IOAPIC_IOREDTBL_BASE 0x10

#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_ARB 0x02

#define IOAPIC_DELMOD_FIXED 0b000
#define IOAPIC_DELMOD_LPRIO 0b001
#define IOAPIC_DELMOD_NMI 0b100

#define IOAPIC_MASKED (1 << 16)
#define IOAPIC_TRIG_LEVEL (1 << 15)
#define IOAPIC_INTPOL_L (1 << 13)
#define IOAPIC_DESTMOD_LOGIC (1 << 11)

#define IOAPIC_BASE_VADDR 0x2000

#define IOAPIC_REG_SEL *((volatile u32_t*)(_ioapic_base + IOAPIC_IOREGSEL))
#define IOAPIC_REG_WIN *((volatile u32_t*)(_ioapic_base + IOAPIC_IOWIN))


#define LVT_ENTRY_LINT0(vector) (LVT_DELIVERY_FIXED | vector)

// Pin LINT#1 is configured for relaying NMI, but we masked it here as I think
//  it is too early for that
// LINT#1 *must* be edge trigged (Intel manual vol3. 10-14)
#define LVT_ENTRY_LINT1 (LVT_DELIVERY_NMI | LVT_MASKED | LVT_TRIGGER_EDGE)
#define LVT_ENTRY_ERROR(vector) (LVT_DELIVERY_FIXED | vector)


static volatile ptr_t _ioapic_base;
static volatile ptr_t _apic_base;

void
apic_write_reg(unsigned int reg, unsigned int val)
{
    *(unsigned int*)(_apic_base + reg) = val;
}

void
isrm_notify_eoi(cpu_t id, int iv)
{
    // for all external interrupts except the spurious interrupt
    //  this is required by Intel Manual Vol.3A, section 10.8.1 & 10.8.5
    if (iv >= IV_EX_BEGIN && iv != APIC_SPIV_IV) {
        *(unsigned int*)(_apic_base + APIC_EOI) = 0;
    }
}

unsigned int
apic_read_reg(unsigned int reg)
{
    return *(unsigned int*)(_apic_base + (reg));
}


static void
apic_setup_lvts()
{
    apic_write_reg(APIC_LVT_LINT0, LVT_ENTRY_LINT0(APIC_LINT0_IV));
    apic_write_reg(APIC_LVT_LINT1, LVT_ENTRY_LINT1);
    apic_write_reg(APIC_LVT_ERROR, LVT_ENTRY_ERROR(APIC_ERROR_IV));
}

static void
apic_init()
{
    // ensure that external interrupt is disabled
    cpu_disable_interrupt();

    // Make sure the APIC is there
    //  FUTURE: Use 8259 as fallback

    // FIXME apic abstraction as local interrupt controller
    // assert_msg(cpu_has_apic(), "No APIC detected!");

    // As we are going to use APIC, disable the old 8259 PIC
    pic_disable();

    _apic_base = ioremap(__APIC_BASE_PADDR, 4096);

    // Hardware enable the APIC
    // By setting bit 11 of IA32_APIC_BASE register
    // Note: After this point, you can't disable then re-enable it until a
    // reset (i.e., reboot)
    asm volatile("movl %0, %%ecx\n"
                 "rdmsr\n"
                 "orl %1, %%eax\n"
                 "wrmsr\n" ::"i"(IA32_MSR_APIC_BASE),
                 "i"(IA32_APIC_ENABLE)
                 : "eax", "ecx", "edx");

    // Print the basic information of our current local APIC
    u32_t apic_id = apic_read_reg(APIC_IDR) >> 24;
    u32_t apic_ver = apic_read_reg(APIC_VER);

    kprintf(KINFO "ID: %x, Version: %x, Max LVT: %u",
            apic_id,
            apic_ver & 0xff,
            (apic_ver >> 16) & 0xff);

    // initialize the local vector table (LVT)
    apic_setup_lvts();

    // initialize priority registers

    // set the task priority to the lowest possible, so all external interrupts
    // are acceptable
    //   Note, the lowest possible priority class is 2, not 0, 1, as they are
    //   reserved for internal interrupts (vector 0-31, and each p-class
    //   resposible for 16 vectors). See Intel Manual Vol. 3A, 10-29
    apic_write_reg(APIC_TPR, APIC_PRIORITY(2, 0));

    // enable APIC
    u32_t spiv = apic_read_reg(APIC_SPIVR);

    // install our handler for spurious interrupt.
    spiv = (spiv & ~0xff) | APIC_SPIV_APIC_ENABLE | APIC_SPIV_IV;
    apic_write_reg(APIC_SPIVR, spiv);
}

static void
ioapic_init()
{
    acpi_context* acpi_ctx = acpi_get_context();

    _ioapic_base =
        ioremap(acpi_ctx->madt.ioapic->ioapic_addr & ~0xfff, 4096);
}

static void
ioapic_write(u8_t sel, u32_t val)
{
    IOAPIC_REG_SEL = sel;
    IOAPIC_REG_WIN = val;
}

static u32_t
ioapic_read(u8_t sel)
{
    IOAPIC_REG_SEL = sel;
    return IOAPIC_REG_WIN;
}

static void
__ioapic_install_line(struct irq_domain *domain, irq_t irq)
{
    struct irq_line_wire *line;
    u8_t reg_sel;
    u32_t ioapic_fg;

    line = irq->line;
    reg_sel = IOAPIC_IOREDTBL_BASE + line->domain_mapped * 2;
    ioapic_fg = IOAPIC_DELMOD_FIXED;

    // Write low 32 bits
    ioapic_write(reg_sel, (irq->vector | ioapic_fg) & 0x1FFFF);

    // Write high 32 bits
    ioapic_write(reg_sel + 1, 0);
}

static int
__ioapic_translate_irq(struct irq_domain *domain, irq_t irq, void *irq_extra)
{
    struct irq_line_wire *line;

    if (irq->type == IRQ_LINE) {
        line = irq->line;
        line->domain_mapped = acpi_gsimap(line->domain_local);
    }

    return 0;
}

static void
__external_irq_dispatch(const struct hart_state* state)
{
    irq_t irq;

    irq = irq_find(irq_get_default_domain(), hart_vector_stamp(state));

    assert(irq);
    irq_serve(irq, state);
}

static int
__ioapic_install_irq(struct irq_domain *domain, irq_t irq)
{
    irq->vector = isrm_ivexalloc(__external_irq_dispatch);

    if (irq->type == IRQ_MESSAGE) {
        irq->msi->wr_addr = __APIC_BASE_PADDR;
    }
    else {
        __ioapic_install_line(domain, irq);
    }

    irq_record(domain, irq);
    
    return 0;
}


static struct irq_domain_ops apic_domain_ops = {
    .map_irq = __ioapic_translate_irq,
    .install_irq = __ioapic_install_irq
};

static int
apic_device_create(struct device_def* def, morph_t* morph)
{
    int err;
    struct device* dev;
    struct irq_domain* domain;

    apic_init();
    ioapic_init();

    dev = device_allocsys(NULL, NULL);
    domain = irq_create_domain(dev, &apic_domain_ops);

    irq_set_default_domain(domain);
    register_device(dev, &def->class, "apic");
    irq_attach_domain(NULL, domain);
    
    return 0;
}

static struct device_def apic_devdef = {
    def_device_class(INTEL, CFG, INTC),
    def_device_name("x86 APIC"),
    def_on_create(apic_device_create)
};
EXPORT_DEVICE(x86_apic, &apic_devdef, load_sysconf);