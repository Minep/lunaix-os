#ifndef __LUNAIX_ARCH_SYSCALL_UTILS_H
#define __LUNAIX_ARCH_SYSCALL_UTILS_H

#include <lunaix/types.h>
#include <klibc/string.h>

static inline void
convert_valist(va_list* ap_ref, sc_va_list syscall_ap)
{
#ifdef CONFIG_ARCH_X86_64
    memcpy(ap_ref, syscall_ap, sizeof(va_list));
#else
    *ap_ref = *syscall_ap;
#endif
}

#endif /* __LUNAIX_ARCH_SYSCALL_UTILS_H */
