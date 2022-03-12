#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>
#include <stdint.h>

#define IDT_ENTRY 256

uint64_t _idt[IDT_ENTRY];
uint16_t _idt_limit = sizeof(_idt) - 1;

void _set_idt_entry(uint32_t vector, uint16_t seg_selector, void (*isr)(), uint8_t dpl) {
    uintptr_t offset = (uintptr_t)isr;
    _idt[vector] = (offset & 0xffff0000) | IDT_ATTR(dpl);
    _idt[vector] <<= 32;
    _idt[vector] |= (seg_selector << 16) | (offset & 0x0000ffff);
}


void
_init_idt() {
    // CPU defined interrupts
    _set_idt_entry(FAULT_DIVISION_ERROR, 0x08, _asm_isr0, 0);
    _set_idt_entry(FAULT_GENERAL_PROTECTION, 0x08, _asm_isr13, 0);
    _set_idt_entry(FAULT_PAGE_FAULT, 0x08, _asm_isr14, 0);

    _set_idt_entry(APIC_ERROR_IV, 0x08, _asm_isr200, 0);
    _set_idt_entry(APIC_LINT0_IV, 0x08, _asm_isr201, 0);
    _set_idt_entry(APIC_TIMER_IV, 0x08, _asm_isr202, 0);
    _set_idt_entry(APIC_SPIV_IV,  0x08, _asm_isr203, 0);

    _set_idt_entry(RTC_TIMER_IV,  0x08, _asm_isr210, 0);

    // system defined interrupts
    _set_idt_entry(LUNAIX_SYS_PANIC, 0x08, _asm_isr32, 0);
}