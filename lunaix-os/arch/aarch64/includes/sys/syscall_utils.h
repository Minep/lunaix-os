#ifndef __LUNAIX_ARCH_SYSCALL_UTILS_H
#define __LUNAIX_ARCH_SYSCALL_UTILS_H

#include <klibc/string.h>

static inline void
convert_valist(va_list* ap_ref, sc_va_list syscall_ap)
{
    memcpy(ap_ref, syscall_ap, sizeof(*ap_ref));
}

#endif /* __LUNAIX_SYSCALL_UTILS_H */
