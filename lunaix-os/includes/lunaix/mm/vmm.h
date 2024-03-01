#ifndef __LUNAIX_VMM_H
#define __LUNAIX_VMM_H

#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/vmtlb.h>
#include <lunaix/process.h>
#include <lunaix/types.h>

/**
 * @brief 初始化虚拟内存管理器
 *
 */
void
vmm_init();

static inline void 
vmm_set_ptes_contig(pte_t* ptep, pte_t pte, size_t lvl_size, size_t n)
{
    do {
        set_pte(ptep, pte);
        pte_val(pte) += lvl_size;
        ptep++;
    } while (--n > 0);
}

static inline void 
vmm_set_ptes(pte_t* ptep, pte_t pte, size_t n)
{
    do {
        set_pte(ptep, pte);
        ptep++;
    } while (--n > 0);
}


static inline void 
vmm_unset_ptes(pte_t* ptep, size_t n)
{
    do {
        set_pte(ptep, null_pte);
        ptep++;
    } while (--n > 0);
}

pte_t
vmm_tryptep(pte_t* ptep, size_t lvl_size);

/**
 * @brief 在指定的虚拟地址空间里查找一个映射
 *
 * @param mnt 地址空间锚定点
 * @param va 虚拟地址
 * @param mapping 映射相关属性
 * @return int
 */
static inline bool
vmm_lookupat(ptr_t mnt, ptr_t va, pte_t* pte_out)
{
    pte_t pte = vmm_tryptep(mkptep_va(mnt, va), LFT_SIZE);
    *pte_out = pte;

    return !pte_isnull(pte);
}

/**
 * @brief 挂载另一个虚拟地址空间至当前虚拟地址空间
 *
 * @param pde 页目录的物理地址
 * @return ptr_t
 */
ptr_t
vms_mount(ptr_t mnt, ptr_t pde);

/**
 * @brief 卸载已挂载的虚拟地址空间
 *
 */
ptr_t
vms_unmount(ptr_t mnt);

static inline ptr_t 
mount_page(ptr_t mnt, ptr_t pa) {
    assert(pa);
    pte_t* ptep = mkptep_va(VMS_SELF, mnt);
    set_pte(ptep, mkpte(pa, KERNEL_DATA));

    tlb_flush_kernel(mnt);
    return mnt;
}

static inline ptr_t 
unmount_page(ptr_t mnt) {
    pte_t* ptep = mkptep_va(VMS_SELF, mnt);
    set_pte(ptep, null_pte);

    tlb_flush_kernel(mnt);
    return mnt;
}

/**
 * @brief 将当前地址空间的虚拟地址转译为物理地址。
 *
 * @param va 虚拟地址
 * @return void*
 */
static inline ptr_t
vmm_v2p(ptr_t va)
{
    pte_t* ptep = mkptep_va(VMS_SELF, va);
    return pte_paddr(pte_at(ptep)) + va_offset(va);
}

void
vmap_set_start(ptr_t start_addr);

#endif /* __LUNAIX_VMM_H */
