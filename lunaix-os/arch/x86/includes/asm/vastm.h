#ifndef __LUNAIX_ARCH_VASTM_FLAT_H
#define __LUNAIX_ARCH_VASTM_FLAT_H

#include "asm/x86_cpu.h"

// TODO page + offset
#define phy_to_virt(page)   (page)


static inline unsigned long
x86_current_vas()
{
    return cpu_ldvmspace() & (-1UL << 12);
}

#define current_vas() x86_current_vas()

#endif
