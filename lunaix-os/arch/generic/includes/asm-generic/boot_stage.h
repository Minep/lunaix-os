#ifndef __LUNAIX_ARCH_GENERIC_BOOT_STAGE_H
#define __LUNAIX_ARCH_GENERIC_BOOT_STAGE_H

#define boot_text __attribute__((section(".boot.text")))
#define boot_data __attribute__((section(".boot.data")))

/*
    Bridge the far symbol to the vicinity.

    Which is a workaround for relocation 
    issue where symbol define in kernel 
    code is too far away from the boot code.
*/
#ifdef CONFIG_ARCH_BITS_64
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

#endif /* __LUNAIX_ARCH_GENERIC_BOOT_STAGE_H */
