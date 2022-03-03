#ifndef __LUNAIX_KALLOC_H
#define __LUNAIX_KALLOC_H

#include <stddef.h>

int
kalloc_init();

/**
 * @brief Allocate a space in kernel heap.This is NOT the same as kmalloc in Linux! 
 * LunaixOS does NOT guarantee the continuity in physical pages.
 * 
 * @param size 
 * @return void* 
 */
void*
kmalloc(size_t size);

/**
 * @brief calloc for kernel heap. A wrapper for kmalloc
 * 
 * @param size 
 * @return void* 
 */
void*
kcalloc(size_t size);

/**
 * @brief free for kernel heap
 * 
 * @param size 
 * @return void* 
 */
void
kfree(void* ptr);

#endif /* __LUNAIX_KALLOC_H */
