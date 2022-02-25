#ifndef __LUNAIX_PMM_H
#define __LUNAIX_PMM_H
// Physical memory manager

#include <stdint.h>
#include <stddef.h>

#define PM_PAGE_SIZE            4096
#define PM_BMP_MAX_SIZE        (128 * 1024)

/**
 * @brief 标注物理页为可使用
 * 
 * @param ppn page number
 */
void pmm_mark_page_free(uintptr_t ppn);

/**
 * @brief 标注物理页为已占用
 * 
 * @param ppn 
 */
void pmm_mark_page_occupied(uintptr_t ppn);

/**
 * @brief 标注多个连续的物理页为可用
 * 
 * @param start_ppn 起始PPN
 * @param page_count 数量
 */
void pmm_mark_chunk_free(uintptr_t start_ppn, size_t page_count);

/**
 * @brief 标注多个连续的物理页为已占用
 * 
 * @param start_ppn 起始PPN
 * @param page_count 数量
 */
void pmm_mark_chunk_occupied(uintptr_t start_ppn, size_t page_count);

/**
 * @brief 分配一个可用的物理页
 * 
 * @return void* 可用的页地址，否则为 NULL
 */
void* pmm_alloc_page();

/**
 * @brief 初始化物理内存管理器
 * 
 * @param mem_upper_lim 最大可用内存地址
 */
void pmm_init(uintptr_t mem_upper_lim);


/**
 * @brief 释放一个已分配的物理页，假若页地址不存在，则无操作。
 * 
 * @param page 页地址
 * @return 是否成功
 */
int pmm_free_page(void* page);


#endif /* __LUNAIX_PMM_H */
