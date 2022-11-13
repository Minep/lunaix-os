#ifndef __LUNAIX_PAGE_H
#define __LUNAIX_PAGE_H
#include <lunaix/common.h>
#include <lunaix/types.h>

#define PG_MAX_ENTRIES 1024U
#define PG_LAST_TABLE PG_MAX_ENTRIES - 1
#define PG_FIRST_TABLE 0

#define PTE_NULL 0

#define P2V(paddr) ((uintptr_t)(paddr) + KERNEL_MM_BASE)
#define V2P(vaddr) ((uintptr_t)(vaddr)-KERNEL_MM_BASE)

#define PG_ALIGN(addr) ((uintptr_t)(addr)&0xFFFFF000UL)

#define L1_INDEX(vaddr) (u32_t)(((uintptr_t)(vaddr)&0xFFC00000UL) >> 22)
#define L2_INDEX(vaddr) (u32_t)(((uintptr_t)(vaddr)&0x003FF000UL) >> 12)
#define PG_OFFSET(vaddr) (u32_t)((uintptr_t)(vaddr)&0x00000FFFUL)

#define GET_PT_ADDR(pde) PG_ALIGN(pde)
#define GET_PG_ADDR(pte) PG_ALIGN(pte)

#define PG_DIRTY(pte) ((pte & (1 << 6)) >> 6)
#define PG_ACCESSED(pte) ((pte & (1 << 5)) >> 5)

#define IS_CACHED(entry) ((entry & 0x1))

#define PG_PRESENT (0x1)
#define PG_WRITE (0x1 << 1)
#define PG_ALLOW_USER (0x1 << 2)
#define PG_WRITE_THROUGH (1 << 3)
#define PG_DISABLE_CACHE (1 << 4)
#define PG_PDE_4MB (1 << 7)

#define NEW_L1_ENTRY(flags, pt_addr)                                           \
    (PG_ALIGN(pt_addr) | (((flags) | PG_WRITE_THROUGH) & 0xfff))
#define NEW_L2_ENTRY(flags, pg_addr) (PG_ALIGN(pg_addr) | ((flags)&0xfff))

#define V_ADDR(pd, pt, offset) ((pd) << 22 | (pt) << 12 | (offset))
#define P_ADDR(ppn, offset) ((ppn << 12) | (offset))

#define PG_ENTRY_FLAGS(entry) ((entry)&0xFFFU)
#define PG_ENTRY_ADDR(entry) ((entry) & ~0xFFFU)

#define HAS_FLAGS(entry, flags) ((PG_ENTRY_FLAGS(entry) & (flags)) == flags)
#define CONTAINS_FLAGS(entry, flags) (PG_ENTRY_FLAGS(entry) & (flags))

#define PG_PREM_R PG_PRESENT
#define PG_PREM_RW PG_PRESENT | PG_WRITE
#define PG_PREM_UR PG_PRESENT | PG_ALLOW_USER
#define PG_PREM_URW PG_PRESENT | PG_WRITE | PG_ALLOW_USER

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
    uintptr_t va;
    // 物理页码（如果不存在映射，则为0）
    u32_t pn;
    // 物理页地址（如果不存在映射，则为0）
    uintptr_t pa;
    // 映射的flags
    uint16_t flags;
    // PTE地址
    x86_pte_t* pte;
} v_mapping;

typedef struct
{
    x86_pte_t entry[PG_MAX_ENTRIES];
} __attribute__((packed)) x86_page_table;

extern void __pg_mount_point;

/* 四个页挂载点，两个页目录挂载点： 用于临时创建&编辑页表 */
#define PG_MOUNT_RANGE(l1_index) (701 <= l1_index && l1_index <= 703)
#define PD_MOUNT_1 (KERNEL_MM_BASE + MEM_4MB)
#define PG_MOUNT_BASE (PD_MOUNT_1 + MEM_4MB)
#define PG_MOUNT_1 (PG_MOUNT_BASE)
#define PG_MOUNT_2 (PG_MOUNT_BASE + 0x1000)
#define PG_MOUNT_3 (PG_MOUNT_BASE + 0x2000)
#define PG_MOUNT_4 (PG_MOUNT_BASE + 0x3000)

#define PD_REFERENCED L2_BASE_VADDR

#define CURPROC_PTE(vpn)                                                       \
    (&((x86_page_table*)(PD_MOUNT_1 | (((vpn)&0xffc00) << 2)))                 \
        ->entry[(vpn)&0x3ff])
#define PTE_MOUNTED(mnt, vpn)                                                  \
    (((x86_page_table*)((mnt) | (((vpn)&0xffc00) << 2)))->entry[(vpn)&0x3ff])

#endif /* __LUNAIX_PAGE_H */
