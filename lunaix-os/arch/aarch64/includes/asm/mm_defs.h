#ifndef __LUNAIX_MM_DEFS_H
#define __LUNAIX_MM_DEFS_H

#include "mempart.h"
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

#define KSTACK_PAGES            4
#define KEXEC_RSVD              32

#define KSTACK_SIZE             (KSTACK_PAGES * PAGE_SIZE)

#define kernel_addr(addr)       ((addr) >= KERNEL_RESIDENT || (addr) < USR_EXEC)

#define to_kphysical(k_va)      ((ptr_t)(k_va) - KERNEL_IMG)
#define to_kvirtual(k_pa)       ((ptr_t)(k_pa) + KERNEL_IMG)

#endif /* __LUNAIX_MM_DEFS_H */
