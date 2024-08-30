#ifndef __LUNAIX_BSTAGE_H
#define __LUNAIX_BSTAGE_H
#include <lunaix/types.h>
#include <lunaix/boot_generic.h>

extern ptr_t __multiboot_addr;

extern u8_t __kboot_start[];
extern u8_t __kboot_end[];

#define boot_text __attribute__((section(".boot.text")))
#define boot_data __attribute__((section(".boot.data")))
#define boot_bss  __attribute__((section(".boot.bss")))

/*
    Bridge the far symbol to the vicinity.

    Which is a workaround for relocation 
    issue where symbol define in kernel 
    code is too far away from the boot code.
*/
#ifdef CONFIG_ARCH_X86_64
#define __bridge_farsym(far_sym)        \
    asm(                                \
        ".section .boot.data\n"         \
        ".align 8\n"                    \
        ".globl __lc_" #far_sym "\n"    \
        "__lc_" #far_sym ":\n"          \
        ".8byte " #far_sym "\n"         \
        ".previous\n"                   \
    );                                  \
    extern unsigned long __lc_##far_sym[];
#define bridge_farsym(far_sym)  __bridge_farsym(far_sym)

#define ___far(far_sym)  (__lc_##far_sym[0])
#define __far(far_sym)  ___far(far_sym)

#else
#define __bridge_farsym(far_sym)    extern unsigned long far_sym[]
#define ___far(far_sym)             ((ptr_t)far_sym)
#define bridge_farsym(far_sym)      __bridge_farsym(far_sym);
#define __far(far_sym)              ___far(far_sym)

#endif

ptr_t 
remap_kernel();

#endif /* __LUNAIX_BSTAGE_H */
