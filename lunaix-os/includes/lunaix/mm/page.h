#ifndef __LUNAIX_PAGE_H
#define __LUNAIX_PAGE_H
#include <lunaix/types.h>

#define PG_SIZE_BITS 12
#define PG_SIZE (1 << PG_SIZE_BITS)

#define PG_MAX_ENTRIES 1024U
#define PG_LAST_TABLE PG_MAX_ENTRIES - 1
#define PG_FIRST_TABLE 0

#define PTE_NULL 0

#define PG_ALIGN(addr) ((ptr_t)(addr)&0xFFFFF000UL)
#define PG_MOD(addr) ((ptr_t)(addr) & (PG_SIZE - 1))
#define PG_ALIGNED(addr) (!((ptr_t)(addr)&0x00000FFFUL))
#define PN(addr) (((ptr_t)(addr) >> 12))

#define L1_INDEX(vaddr) (u32_t)(((ptr_t)(vaddr)&0xFFC00000UL) >> 22)
#define L2_INDEX(vaddr) (u32_t)(((ptr_t)(vaddr)&0x003FF000UL) >> 12)
#define PG_OFFSET(vaddr) (u32_t)((ptr_t)(vaddr)&0x00000FFFUL)

#define GET_PT_ADDR(pde) PG_ALIGN(pde)
#define GET_PG_ADDR(pte) PG_ALIGN(pte)

#define IS_CACHED(entry) ((entry & 0x1))

#define PG_PRESENT (0x1)
#define PG_DIRTY (1 << 6)
#define PG_ACCESSED (1 << 5)
#define PG_WRITE (0x1 << 1)
#define PG_ALLOW_USER (0x1 << 2)
#define PG_WRITE_THROUGH (1 << 3)
#define PG_DISABLE_CACHE (1 << 4)
#define PG_PDE_4MB (1 << 7)

#define PG_IS_DIRTY(pte) ((pte)&PG_DIRTY)
#define PG_IS_ACCESSED(pte) ((pte)&PG_ACCESSED)
#define PG_IS_PRESENT(pte) ((pte)&PG_PRESENT)

#define NEW_L1_ENTRY(flags, pt_addr)                                           \
    (PG_ALIGN(pt_addr) | (((flags) | PG_WRITE_THROUGH) & 0xfff))
#define NEW_L2_ENTRY(flags, pg_addr) (PG_ALIGN(pg_addr) | ((flags)&0xfff))

#define V_ADDR(pd, pt, offset) ((pd) << 22 | (pt) << 12 | (offset))
#define P_ADDR(ppn, offset) ((ppn << 12) | (offset))

#define PG_ENTRY_FLAGS(entry) ((entry)&0xFFFU)
#define PG_ENTRY_ADDR(entry) ((entry) & ~0xFFFU)

#define HAS_FLAGS(entry, flags) ((PG_ENTRY_FLAGS(entry) & (flags)) == flags)
#define CONTAINS_FLAGS(entry, flags) (PG_ENTRY_FLAGS(entry) & (flags))

#define PG_PREM_R (PG_PRESENT)
#define PG_PREM_RW (PG_PRESENT | PG_WRITE)
#define PG_PREM_UR (PG_PRESENT | PG_ALLOW_USER)
#define PG_PREM_URW (PG_PRESENT | PG_WRITE | PG_ALLOW_USER)

// 用于对PD进行循环映射，因为我们可能需要对PD进行频繁操作，我们在这里禁用TLB缓存
#define T_SELF_REF_PERM PG_PREM_RW | PG_DISABLE_CACHE | PG_WRITE_THROUGH

// 页目录的虚拟基地址，可以用来访问到各个PDE
#define L1_BASE_VADDR 0xFFFFF000U

// 页表的虚拟基地址，可以用来访问到各个PTE
#define L2_BASE_VADDR 0xFFC00000U

// 用来获取特定的页表的虚拟地址
#define L2_VADDR(pd_offset) (L2_BASE_VADDR | (pd_offset << 12))

typedef unsigned long ptd_t;
typedef unsigned long pt_t;
typedef unsigned int pt_attr;
typedef u32_t x86_pte_t;

/**
 * @brief 虚拟映射属性
 *
 */
typedef struct
{
    // 虚拟页地址
    ptr_t va;
    // 物理页码（如果不存在映射，则为0）
    u32_t pn;
    // 物理页地址（如果不存在映射，则为0）
    ptr_t pa;
    // 映射的flags
    u16_t flags;
    // PTE地址
    x86_pte_t* pte;
} v_mapping;

typedef struct
{
    x86_pte_t entry[PG_MAX_ENTRIES];
} __attribute__((packed, aligned(4))) x86_page_table;

/* 四个页挂载点，两个页目录挂载点： 用于临时创建&编辑页表 */
#define PG_MOUNT_RANGE(l1_index) (701 <= l1_index && l1_index <= 703)

/*
    当前进程内存空间挂载点
*/
// #define VMS_SELF L2_BASE_VADDR

#define PTE_MOUNTED(mnt, vpn)                                                  \
    (((x86_page_table*)((mnt) | (((vpn)&0xffc00) << 2)))->entry[(vpn)&0x3ff])

#endif /* __LUNAIX_PAGE_H */
