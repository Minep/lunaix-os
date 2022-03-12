#ifndef __LUNAIX_VMM_H
#define __LUNAIX_VMM_H
#include <lunaix/mm/page.h>
#include <stddef.h>
#include <stdint.h>
// Virtual memory manager

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
 * @brief 尝试建立一个映射关系。映射指定的物理页地址至虚拟页地址，如果指定的虚拟页地址已被占用
 * 则尝试寻找新的可用地址（改地址总是大于指定的地址）。
 *
 * @param vpn 虚拟页地址
 * @param pa 物理页地址
 * @param dattr PDE 的属性
 * @param tattr PTE 的属性
 * @return 虚拟页地址，如不成功，则为 NULL
 */
void*
vmm_map_page(void* va, void* pa, pt_attr tattr);

/**
 * @brief 建立一个映射关系，映射指定的物理页地址至虚拟页地址。如果指定的虚拟页地址已被占用，
 * 则覆盖。
 *
 * @param va 虚拟页地址
 * @param pa 物理页地址
 * @param dattr PDE 的属性
 * @param tattr PTE 的属性
 * @return 虚拟页地址
 */
void*
vmm_fmap_page(void* va, void* pa, pt_attr tattr);

/**
 * @brief 尝试为一个虚拟页地址创建一个可用的物理页映射
 *
 * @param va 虚拟页地址
 * @return 物理页地址，如不成功，则为 NULL
 */
void*
vmm_alloc_page(void* va, pt_attr tattr);


/**
 * @brief 尝试分配多个连续的虚拟页
 * 
 * @param va 起始虚拟地址
 * @param sz 大小（必须为4K对齐）
 * @param tattr 属性
 * @return int 是否成功
 */
int
vmm_alloc_pages(void* va, size_t sz, pt_attr tattr);

/**
 * @brief 设置一个映射，如果映射已存在，则忽略。
 * 
 * @param va 
 * @param pa 
 * @param attr 
 */
void
vmm_set_mapping(void* va, void* pa, pt_attr attr);

/**
 * @brief 删除一个映射
 *
 * @param vpn
 */
void
vmm_unmap_page(void* va);

/**
 * @brief 将虚拟地址翻译为其对应的物理映射
 *
 * @param va 虚拟地址
 * @return void* 物理地址，如映射不存在，则为NULL
 */
void*
vmm_v2p(void* va);

/**
 * @brief 查找一个映射
 *
 * @param va 虚拟地址
 * @return v_mapping 映射相关属性
 */
v_mapping
vmm_lookup(void* va);

#endif /* __LUNAIX_VMM_H */
