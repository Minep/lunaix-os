#ifndef __LUNAIX_PAGE_H
#define __LUNAIX_PAGE_H
#include <stdint.h>
#include <lunaix/constants.h>

#define PG_MAX_ENTRIES              1024U
#define PG_LAST_TABLE               PG_MAX_ENTRIES - 1
#define PG_FIRST_TABLE              0

#define P2V(paddr)          ((uintptr_t)(paddr)  +  HIGHER_HLF_BASE)
#define V2P(vaddr)          ((uintptr_t)(vaddr)  -  HIGHER_HLF_BASE)

#define PG_ALIGN(addr)      ((uintptr_t)(addr)   & 0xFFFFF000UL)

#define PD_INDEX(vaddr)     (((uintptr_t)(vaddr) & 0xFFC00000UL) >> 22)
#define PT_INDEX(vaddr)     (((uintptr_t)(vaddr) & 0x003FF000UL) >> 12)
#define PG_OFFSET(vaddr)    ((uintptr_t)(vaddr)  & 0x00000FFFUL)

#define GET_PT_ADDR(pde)    PG_ALIGN(pde)
#define GET_PG_ADDR(pte)    PG_ALIGN(pte)

#define PG_DIRTY(pte)           ((pte & (1 << 6)) >> 6)
#define PG_ACCESSED(pte)        ((pte & (1 << 5)) >> 5)

#define IS_CACHED(entry)    ((entry & 0x1))

#define PG_PRESENT              (0x1)
#define PG_WRITE                (0x1 << 1)
#define PG_ALLOW_USER           (0x1 << 2)
#define PG_WRITE_THROUGHT       (1 << 3)
#define PG_DISABLE_CACHE        (1 << 4)
#define PG_PDE_4MB              (1 << 7)

#define PDE(flags, pt_addr)     (PG_ALIGN(pt_addr) | ((flags) & 0xfff))
#define PTE(flags, pg_addr)     (PG_ALIGN(pg_addr) | ((flags) & 0xfff))

#define V_ADDR(pd, pt, offset)  ((pd) << 22 | (pt) << 12 | (offset))
#define P_ADDR(ppn, offset)     ((ppn << 12) | (offset))

// 用于对PD进行循环映射，因为我们可能需要对PD进行频繁操作，我们在这里禁用TLB缓存
#define PG_PREM_R              PG_PRESENT
#define PG_PREM_RW             PG_PRESENT | PG_WRITE
#define PG_PREM_UR             PG_PRESENT | PG_ALLOW_USER
#define PG_PREM_URW            PG_PRESENT | PG_WRITE | PG_ALLOW_USER
#define T_SELF_REF_PERM        PG_PREM_RW | PG_DISABLE_CACHE


// 页目录的虚拟基地址，可以用来访问到各个PDE
#define PTD_BASE_VADDR                0xFFFFF000U

// 页表的虚拟基地址，可以用来访问到各个PTE
#define PT_BASE_VADDR                 0xFFC00000U

// 用来获取特定的页表的虚拟地址
#define PT_VADDR(pd_offset)           (PT_BASE_VADDR | (pd_offset << 12))

typedef unsigned int ptd_t;
typedef unsigned int pt_t;
typedef unsigned int pt_attr;


#endif /* __LUNAIX_PAGE_H */
