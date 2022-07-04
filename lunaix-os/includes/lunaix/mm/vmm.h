#ifndef __LUNAIX_VMM_H
#define __LUNAIX_VMM_H
#include <lunaix/mm/page.h>
#include <lunaix/process.h>
#include <stddef.h>
#include <stdint.h>
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
vmm_set_mapping(uintptr_t mnt,
                uintptr_t va,
                uintptr_t pa,
                pt_attr attr,
                int options);

/**
 * @brief 删除一个映射
 *
 * @param mnt
 * @param pid
 * @param va
 * @return int
 */
uintptr_t
vmm_del_mapping(uintptr_t mnt, uintptr_t va);

/**
 * @brief 查找一个映射
 *
 * @param va 虚拟地址
 * @return v_mapping 映射相关属性
 */
int
vmm_lookup(uintptr_t va, v_mapping* mapping);

/**
 * @brief (COW) 为虚拟页创建副本。
 *
 * @return void* 包含虚拟页副本的物理页地址。
 *
 */
void*
vmm_dup_page(pid_t pid, void* pa);

void*
vmm_dup_vmspace(pid_t pid);

/**
 * @brief 挂载另一个虚拟地址空间至当前虚拟地址空间
 *
 * @param pde 页目录的物理地址
 * @return void*
 */
void*
vmm_mount_pd(uintptr_t mnt, void* pde);

/**
 * @brief 卸载已挂载的虚拟地址空间
 *
 */
void*
vmm_unmount_pd(uintptr_t mnt);

void*
vmm_ioremap(uintptr_t paddr, size_t size);

void*
vmm_next_free(uintptr_t start, int options);

/**
 * @brief 将连续的物理地址空间映射到内核虚拟地址空间
 *
 * @param paddr 物理地址空间的基地址
 * @param size 物理地址空间的大小
 * @return void*
 */
void*
vmm_vmap(uintptr_t paddr, size_t size, pt_attr attr);

void*
vmm_v2p(void* va);

#endif /* __LUNAIX_VMM_H */
