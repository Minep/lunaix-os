#ifndef __LUNAIX_VMM_H
#define __LUNAIX_VMM_H
#include <lunaix/mm/page.h>
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
 * @brief 创建一个页目录
 *
 * @return ptd_entry* 页目录的物理地址，随时可以加载进CR3
 */
x86_page_table*
vmm_init_pd();

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
vmm_set_mapping(ptr_t mnt, ptr_t va, ptr_t pa, pt_attr attr, int options);

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

/**
 * @brief 在当前虚拟地址空间里查找一个映射
 *
 * @param va 虚拟地址
 * @param mapping 映射相关属性
 */
int
vmm_lookup(ptr_t va, v_mapping* mapping);

/**
 * @brief 在指定的虚拟地址空间里查找一个映射
 *
 * @param mnt 地址空间锚定点
 * @param va 虚拟地址
 * @param mapping 映射相关属性
 * @return int
 */
int
vmm_lookupat(ptr_t mnt, ptr_t va, v_mapping* mapping);

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
vmm_mount_pd(ptr_t mnt, ptr_t pde);

/**
 * @brief 卸载已挂载的虚拟地址空间
 *
 */
ptr_t
vmm_unmount_pd(ptr_t mnt);

void*
vmm_ioremap(ptr_t paddr, size_t size);

void*
vmm_next_free(ptr_t start, int options);

/**
 * @brief 将当前地址空间的虚拟地址转译为物理地址。
 *
 * @param va 虚拟地址
 * @return void*
 */
ptr_t
vmm_v2p(ptr_t va);

/**
 * @brief 将指定地址空间的虚拟地址转译为物理地址
 *
 * @param mnt 地址空间锚定点
 * @param va 虚拟地址
 * @return void*
 */
ptr_t
vmm_v2pat(ptr_t mnt, ptr_t va);

/*
    表示一个 vmap 区域
    (One must not get confused with vmap_area in Linux!)
*/
struct vmap_area
{
    ptr_t start;
    size_t size;
    pt_attr area_attr;
};

/**
 * @brief 将连续的物理地址空间映射到内核虚拟地址空间
 *
 * @param paddr 物理地址空间的基地址
 * @param size 物理地址空间的大小
 * @return void*
 */
void*
vmap(ptr_t paddr, size_t size, pt_attr attr, int flags);

/**
 * @brief 创建一个 vmap 区域
 *
 * @param paddr
 * @param attr
 * @return ptr_t
 */
struct vmap_area*
vmap_varea(size_t size, pt_attr attr);

/**
 * @brief 在 vmap区域内映射一个单页
 *
 * @param paddr
 * @param attr
 * @return ptr_t
 */
ptr_t
vmap_area_page(struct vmap_area* area, ptr_t paddr, pt_attr attr);

/**
 * @brief 在 vmap区域删除一个已映射的页
 *
 * @param paddr
 * @return ptr_t
 */
ptr_t
vmap_area_rmpage(struct vmap_area* area, ptr_t vaddr);

#endif /* __LUNAIX_VMM_H */
