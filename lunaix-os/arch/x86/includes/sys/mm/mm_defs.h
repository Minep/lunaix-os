#ifndef __LUNAIX_MM_DEFS_H
#define __LUNAIX_MM_DEFS_H

#include "mempart.h"

#define KERNEL_RESIDENT         KERNEL_IMG

#ifndef __LD__

#include "pagetable.h"

/*
    Regardless architecture we need to draw the line very carefully, and must 
    take the size of VM into account. In general, we aims to achieve 
    "sufficiently large" of memory for kernel

    In terms of x86_32:
        * #768~1022 PTEs of PD      (0x00000000c0000000, ~1GiB)
    
    In light of upcomming x86_64 support (for Level 4&5 Paging):
        * #510 entry of PML4        (0x0000ff0000000000, ~512GiB)
        * #510 entry of PML5        (0x01fe000000000000, ~256TiB)
    

    KERNEL_RESIDENT  -  a high-mem region, kernel should be
    KSTACK_PAGES     -  kernel stack, pages allocated to
    KEXEC_RSVD       -  page reserved for kernel images
*/

#ifdef CONFIG_ARCH_X86_64
#   define KSTACK_PAGES            4
#   define KEXEC_RSVD              32
#else
#   define KSTACK_PAGES            3
#   define KEXEC_RSVD              16
#endif

#define KSTACK_SIZE             (KSTACK_PAGES * PAGE_SIZE)

#define kernel_addr(addr)       ((addr) >= KERNEL_RESIDENT || (addr) < USR_EXEC)

#define to_kphysical(k_va)      ((ptr_t)(k_va) - KERNEL_RESIDENT)
#define to_kvirtual(k_pa)       ((ptr_t)(k_pa) - KERNEL_RESIDENT)

#endif

#endif /* __LUNAIX_MM_DEFS_H */
