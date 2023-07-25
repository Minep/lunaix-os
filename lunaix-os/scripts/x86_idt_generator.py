from pathlib import Path

intr_handler_h = "arch/i386/intrhnds.h"
intr_handler_c = "kernel/asm/x86/intrhnds.c"
intr_handler_s = "kernel/asm/x86/intrhnds.S"


# default value for each interrupt vector config

config = {
    '8': {
        'with_errno': True
    },
    '10': {
        'with_errno': True
    },
    '11': {
        'with_errno': True
    },
    '12': {
        'with_errno': True
    },
    '13': {
        'with_errno': True
    },
    '14': {
        'with_errno': True
    },
    '17': {
        'with_errno': True
    },
    '33': {
        'dpl': 3
    },
    'default': {
        'seg': 0x8,
        'dpl': 0,
        'vtype': 'IDT_INTERRUPT',
        'with_errno': False
    }
}


def get_workspace() -> Path:
    file = Path(__file__)
    return file.parent.parent.absolute()


def export_h(src: Path):
    hfile = src.joinpath("includes", intr_handler_h)
    code = '\n'.join(["ISR(%s)" % x for x in range(0, 256)])
    with hfile.open(mode='w+') as f:
        f.write(f'''
#ifndef __LUNAIX_INTRHNDS_H
#define __LUNAIX_INTRHNDS_H
#define ISR(iv) void _asm_isr##iv();
{code}
#endif
''')


def get_vector_config(vec_num, cfg_name):
    vn = str(vec_num)
    if vn not in config:
        return config['default'][cfg_name]
    attr = config[vn]
    if cfg_name not in attr:
        return config['default'][cfg_name]
    return attr[cfg_name]


def export_c(src: Path):
    cfile = src.joinpath(intr_handler_c)
    expr_list = []
    for i in range(0, 256):
        seg = get_vector_config(i, "seg")
        dpl = get_vector_config(i, "dpl")
        vtype = get_vector_config(i, "vtype")
        expr_list.append(
            f'    _set_idt_entry({i}, {seg}, _asm_isr{i}, {dpl}, {vtype});')
    code = '\n'.join(expr_list)
    with cfile.open(mode='w+') as f:
        f.write(f'''
#include <arch/i386/idt.h>
#include <stdint.h>
#include <{intr_handler_h}>

#define IDT_ENTRY 256

u64_t _idt[IDT_ENTRY];
u16_t _idt_limit = sizeof(_idt) - 1;
static inline void
_set_idt_entry(u32_t vector,
               u16_t seg_selector,
               void (*isr)(),
               u8_t dpl,
               u8_t type)
{{
    ptr_t offset = (ptr_t)isr;
    _idt[vector] = (offset & 0xffff0000) | IDT_ATTR(dpl, type);
    _idt[vector] <<= 32;
    _idt[vector] |= (seg_selector << 16) | (offset & 0x0000ffff);
}}

void _init_idt() {{
{code}
}}
''')


def export_s(src: Path):
    sfile = src.joinpath(intr_handler_s)
    expr_list = []
    for i in range(0, 256):
        has_errno = get_vector_config(i, "with_errno")
        expr_list.append(
            f"    isr_template {i}, no_error_code={'0' if has_errno else '1'}")

    code = '\n'.join(expr_list)
    marco = '''.macro isr_template vector, no_error_code=1
    .global _asm_isr\\vector
    .type _asm_isr\\vector, @function
    _asm_isr\\vector:
        .if \\no_error_code
            pushl $0x0
        .endif
        pushl $\\vector
        jmp interrupt_wrapper
.endm'''
    with sfile.open(mode='w+') as f:
        f.write(f'''
#define __ASM__
{marco}
.section .text
{code}
''')


def main():
    src = get_workspace()
    export_h(src)
    export_c(src)
    export_s(src)


if __name__ == "__main__":
    main()
