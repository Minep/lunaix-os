#ifndef __LUNAIX_APIC_H
#define __LUNAIX_APIC_H

#include <lunaix/common.h>
#include <stdint.h>

#define __APIC_BASE_PADDR 0xFEE00000

#define IA32_MSR_APIC_BASE 0x1B
#define IA32_APIC_ENABLE 0x800

/*
 *  Common APIC memory-mapped registers
 *              Ref: Intel Manual, Vol. 3A, Table 10-1
 */

#define APIC_IDR 0x20   // ID Reg
#define APIC_VER 0x30   // Version Reg
#define APIC_TPR 0x80   // Task Priority
#define APIC_APR 0x90   // Arbitration Priority
#define APIC_PPR 0xA0   // Processor Priority
#define APIC_EOI 0xB0   // End-Of-Interrupt
#define APIC_RRD 0xC0   // Remote Read
#define APIC_LDR 0xD0   // Local Destination Reg
#define APIC_DFR 0xE0   // Destination Format Reg
#define APIC_SPIVR 0xF0 // Spurious Interrupt Vector Reg
#define APIC_ISR_BASE                                                          \
    0x100 // Base address for In-Service-Interrupt bitmap register (256bits)
#define APIC_TMR_BASE                                                          \
    0x180 // Base address for Trigger-Mode bitmap register (256bits)
#define APIC_IRR_BASE                                                          \
    0x200 // Base address for Interrupt-Request bitmap register (256bits)
#define APIC_ESR 0x280      // Error Status Reg
#define APIC_ICR_BASE 0x300 // Interrupt Command
#define APIC_LVT_LINT0 0x350
#define APIC_LVT_LINT1 0x360
#define APIC_LVT_ERROR 0x370

// APIC Timer specific
#define APIC_TIMER_LVT 0x320
#define APIC_TIMER_ICR 0x380 // Initial Count
#define APIC_TIMER_CCR 0x390 // Current Count
#define APIC_TIMER_DCR 0x3E0 // Divide Configuration

#define APIC_SPIV_FOCUS_DISABLE 0x200
#define APIC_SPIV_APIC_ENABLE 0x100
#define APIC_SPIV_EOI_BROADCAST 0x1000

#define LVT_DELIVERY_FIXED 0
#define LVT_DELIVERY_NMI (0x4 << 8)
#define LVT_TRIGGER_EDGE (0 << 15)
#define LVT_TRIGGER_LEVEL (1 << 15)
#define LVT_MASKED (1 << 16)
#define LVT_TIMER_ONESHOT (0 << 17)
#define LVT_TIMER_PERIODIC (1 << 17)

// Dividers for timer. See Intel Manual Vol3A. 10-17 (pp. 3207), Figure 10-10
#define APIC_TIMER_DIV1 0b1011
#define APIC_TIMER_DIV2 0b0000
#define APIC_TIMER_DIV4 0b0001
#define APIC_TIMER_DIV8 0b0010
#define APIC_TIMER_DIV16 0b0011
#define APIC_TIMER_DIV32 0b1000
#define APIC_TIMER_DIV64 0b1001
#define APIC_TIMER_DIV128 0b1010

#define APIC_PRIORITY(cls, subcls) (((cls) << 4) | (subcls))

unsigned int
apic_read_reg(unsigned int reg);

void
apic_write_reg(unsigned int reg, unsigned int val);

void
apic_init();

/**
 * @brief Tell the APIC that the handler for current interrupt is finished.
 * This will issue a write action to EOI register.
 *
 */
void
apic_done_servicing();

#endif /* __LUNAIX_APIC_H */
