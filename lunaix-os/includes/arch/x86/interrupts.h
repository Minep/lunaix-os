#ifndef __LUNAIX_INTERRUPTS_H
#define __LUNAIX_INTERRUPTS_H

typedef struct {
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
} __attribute__((packed)) isr_param;

void
_asm_isr0();

void
interrupt_handler(isr_param* param);

#endif /* __LUNAIX_INTERRUPTS_H */
