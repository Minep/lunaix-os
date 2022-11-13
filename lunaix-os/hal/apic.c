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
#include <hal/apic.h>
#include <hal/cpu.h>
#include <hal/pic.h>
#include <hal/rtc.h>

#include <arch/x86/interrupts.h>

#include <lunaix/mm/mmio.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

LOG_MODULE("APIC")

static volatile uintptr_t _apic_base;

void
apic_setup_lvts();

void
apic_init()
{
    // ensure that external interrupt is disabled
    cpu_disable_interrupt();

    // Make sure the APIC is there
    //  FUTURE: Use 8259 as fallback
    assert_msg(cpu_has_apic(), "No APIC detected!");

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

    kprintf(KINFO "ID: %x, Version: %x, Max LVT: %u\n",
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

#define LVT_ENTRY_LINT0(vector) (LVT_DELIVERY_FIXED | vector)

// Pin LINT#1 is configured for relaying NMI, but we masked it here as I think
//  it is too early for that
// LINT#1 *must* be edge trigged (Intel manual vol3. 10-14)
#define LVT_ENTRY_LINT1 (LVT_DELIVERY_NMI | LVT_MASKED | LVT_TRIGGER_EDGE)
#define LVT_ENTRY_ERROR(vector) (LVT_DELIVERY_FIXED | vector)

void
apic_setup_lvts()
{
    apic_write_reg(APIC_LVT_LINT0, LVT_ENTRY_LINT0(APIC_LINT0_IV));
    apic_write_reg(APIC_LVT_LINT1, LVT_ENTRY_LINT1);
    apic_write_reg(APIC_LVT_ERROR, LVT_ENTRY_ERROR(APIC_ERROR_IV));
}

void
apic_done_servicing()
{
    *(unsigned int*)(_apic_base + APIC_EOI) = 0;
}

unsigned int
apic_read_reg(unsigned int reg)
{
    return *(unsigned int*)(_apic_base + (reg));
}

void
apic_write_reg(unsigned int reg, unsigned int val)
{
    *(unsigned int*)(_apic_base + reg) = val;
}