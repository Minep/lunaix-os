#ifndef __LUNAIX_PMM_H
#define __LUNAIX_PMM_H
// Physical memory manager

#include <lunaix/types.h>
#include <stddef.h>
#include <stdint.h>

#define PM_PAGE_SIZE 4096
#define PM_BMP_MAX_SIZE (1024 * 1024)

/**
 * @brief 长久页：不会被缓存，但允许释放
 *
 */
#define PP_FGPERSIST 0x1

/**
 * @brief 锁定页：不会被缓存，不能被释放
 *
 */
#define PP_FGLOCKED 0x2

typedef u32_t pp_attr_t;

struct pp_struct
{
    u32_t ref_counts;
    pp_attr_t attr;
};

/**
 * @brief 标注物理页为可使用
 *
 * @param ppn page number
 */
void
pmm_mark_page_free(ptr_t ppn);

/**
 * @brief 标注物理页为已占用
 *
 * @param ppn
 */
void
pmm_mark_page_occupied(ptr_t ppn, pp_attr_t attr);

/**
 * @brief 标注多个连续的物理页为可用
 *
 * @param start_ppn 起始PPN
 * @param page_count 数量
 */
void
pmm_mark_chunk_free(ptr_t start_ppn, size_t page_count);

/**
 * @brief 标注多个连续的物理页为已占用
 *
 * @param start_ppn 起始PPN
 * @param page_count 数量
 */
void
pmm_mark_chunk_occupied(u32_t start_ppn,
                        size_t page_count,
                        pp_attr_t attr);

/**
 * @brief 分配一个可用的物理页
 *
 * @return void* 可用的页地址，否则为 NULL
 */
ptr_t
pmm_alloc_page(pp_attr_t attr);

/**
 * @brief 分配一个连续的物理内存区域
 *
 * @param owner
 * @param num_pages 区域大小，单位为页
 * @param attr
 * @return ptr_t
 */
ptr_t
pmm_alloc_cpage(size_t num_pages, pp_attr_t attr);

/**
 * @brief 初始化物理内存管理器
 *
 * @param mem_upper_lim 最大可用内存地址
 */
void
pmm_init(ptr_t mem_upper_lim);

struct pp_struct*
pmm_query(ptr_t pa);

/**
 * @brief 释放一个已分配的物理页，假若页地址不存在，则无操作。
 *
 * @param page 页地址
 * @return 是否成功
 */
int
pmm_free_page(ptr_t page);

int
pmm_ref_page(ptr_t page);

#endif /* __LUNAIX_PMM_H */
