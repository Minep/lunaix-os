#ifndef __LUNAIX_DMM_H
#define __LUNAIX_DMM_H
// Dynamic Memory (i.e., heap) Manager

#include <stddef.h>

#define HEAP_INIT_SIZE 4096

typedef struct 
{
    void* start;
    void* brk;
} heap_context_t;


int
dmm_init(heap_context_t* heap);

int
lxsbrk(heap_context_t* heap, void* addr);
void*
lxbrk(heap_context_t* heap, size_t size);

void*
lx_malloc(heap_context_t* heap, size_t size);

void
lx_free(void* ptr);

#endif /* __LUNAIX_DMM_H */
