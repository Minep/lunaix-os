#ifndef __LUNAIX_KALLOC_H
#define __LUNAIX_KALLOC_H

#include <stddef.h>

int
kalloc_init();

/**
 * @brief Allocate a contiguous and un-initialized memory region in kernel heap. 
 * 
 * @remarks 
 *  This is NOT the same as kmalloc in Linux! 
 *  LunaixOS does NOT guarantee the continuity in physical pages.
 * 
 * @param size 
 * @return void* 
 */
void*
lxmalloc(size_t size);

/**
 * @brief Allocate a contiguous and initialized memory region in kernel heap.
 * @param size 
 * @return void*
 */
void*
lxcalloc(size_t n, size_t elem);

/**
 * @brief Free the memory region allocated by kmalloc
 * 
 * @param size 
 * @return void* 
 */
void
lxfree(void* ptr);

#endif /* __LUNAIX_KALLOC_H */
