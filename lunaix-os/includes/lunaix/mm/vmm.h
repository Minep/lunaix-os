#ifndef __LUNAIX_VMM_H
#define __LUNAIX_VMM_H

#include <lunaix/mm/pagetable.h>
#include <lunaix/process.h>
#include <lunaix/types.h>
// Virtual memory manager

#define VMAP_NULL 0

/**
 * @brief 映射模式：忽略已存在映射
 *
 */
#define VMAP_IGNORE 1

/**
 * @brief 映射模式：不作实际映射。该功能用于预留出特定的地址空间
 *
 */
#define VMAP_NOMAP 2

/**
 * @brief 映射页墙：将虚拟地址映射为页墙，忽略给定的物理地址和页属性
 *
 */
#define VMAP_GUARDPAGE 4

/**
 * @brief 规定下一个可用页映射应当限定在指定的4MB地址空间内
 *
 */
#define VALLOC_PDE 1

/**
 * @brief 初始化虚拟内存管理器
 *
 */
void
vmm_init();

/**
 * @brief 在指定地址空间中，添加一个映射
 *
 * @param mnt 地址空间挂载点
 * @param va 虚拟地址
 * @param pa 物理地址
 * @param attr 映射属性
 * @return int
 */
int
vmm_set_mapping(ptr_t mnt, ptr_t va, ptr_t pa, pte_attr_t prot);

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


/**
 * @brief 删除一个映射
 *
 * @param mnt
 * @param pid
 * @param va
 * @return int
 */
ptr_t
vmm_del_mapping(ptr_t mnt, ptr_t va);

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
 * @brief (COW) 为虚拟页创建副本。
 *
 * @return void* 包含虚拟页副本的物理页地址。
 *
 */
ptr_t
vmm_dup_page(ptr_t pa);

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
    cpu_flush_page(mnt);
    return mnt;
}

static inline ptr_t 
unmount_page(ptr_t mnt) {
    pte_t* ptep = mkptep_va(VMS_SELF, mnt);
    set_pte(ptep, null_pte);
    return mnt;
}

void*
vmm_ioremap(ptr_t paddr, size_t size);

void*
vmm_next_free(ptr_t start, int options);

/**
 * @brief 将指定地址空间的虚拟地址转译为物理地址
 *
 * @param mnt 地址空间锚定点
 * @param va 虚拟地址
 * @return void*
 */
ptr_t
vmm_v2pat(ptr_t mnt, ptr_t va);

/**
 * @brief 将当前地址空间的虚拟地址转译为物理地址。
 *
 * @param va 虚拟地址
 * @return void*
 */
static inline ptr_t
vmm_v2p(ptr_t va)
{
    return vmm_v2pat(VMS_SELF, va);
}

/**
 * @brief Maps a number of contiguous ptes in kernel 
 *        address space
 * 
 * @param pte the pte to be mapped
 * @param lvl_size size of the page pointed by the given pte
 * @param n number of ptes
 * @return ptr_t 
 */
ptr_t
vmap_ptes_at(pte_t pte, size_t lvl_size, int n);

/**
 * @brief Maps a number of contiguous ptes in kernel 
 *        address space (leaf page size)
 * 
 * @param pte the pte to be mapped
 * @param n number of ptes
 * @return ptr_t 
 */
static inline ptr_t
vmap_leaf_ptes(pte_t pte, int n)
{
    return vmap_ptes_at(pte, LFT_SIZE, n);
}

/**
 * @brief Maps a contiguous range of physical address 
 *        into kernel address space (leaf page size)
 * 
 * @param paddr start of the physical address range
 * @param size size of the physical range
 * @param prot default protection to be applied
 * @return ptr_t 
 */
static inline ptr_t
vmap(ptr_t paddr, size_t size, pte_attr_t prot)
{
    pte_t _pte = mkpte(paddr, prot);
    return vmap_ptes_at(_pte, LFT_SIZE, leaf_count(size));
}

#endif /* __LUNAIX_VMM_H */
