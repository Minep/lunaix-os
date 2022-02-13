#include <lunaix/arch/idt.h>
#include <lunaix/interrupts/interrupts.h>
#include <lunaix/interrupts/types.h>
#include <stdint.h>

#define IDT_ENTRY 32

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
    _set_idt_entry(FAULT_DIVISION_ERROR, 0x08, _asm_isr0, 0);
}