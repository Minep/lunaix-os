#ifndef __LUNAIX_DMM_H
#define __LUNAIX_DMM_H
// Dynamic Memory (i.e., heap) Manager

#include <stddef.h>

#define HEAP_INIT_SIZE 4096

int
dmm_init();

int
lxsbrk(void* addr);
void*
lxbrk(size_t size);

void*
lx_malloc(size_t size);

void
lx_free(void* ptr);

#endif /* __LUNAIX_DMM_H */
