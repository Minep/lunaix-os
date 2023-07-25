#ifndef __LUNAIX_ABI_H
#define __LUNAIX_ABI_H

/* clang-format off */

#if 0
// templates, new arch should implements these templates
#define store_retval(retval)
#endif

#ifdef __ARCH_IA32
    #include "i386/i386_asm.h"
    #ifndef __ASM__
        #include "i386/i386_abi.h"
    #endif
#endif

/* clang-format on */

#endif /* __LUNAIX_ABI_H */
