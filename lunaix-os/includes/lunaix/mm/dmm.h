#ifndef __LUNAIX_DMM_H
#define __LUNAIX_DMM_H
// Dynamic Memory (i.e., heap) Manager

#include <lunaix/mm/mm.h>
#include <lunaix/process.h>
#include <stddef.h>

#define M_ALLOCATED 0x1
#define M_PREV_FREE 0x2

#define M_NOT_ALLOCATED 0x0
#define M_PREV_ALLOCATED 0x0

#define CHUNK_S(header) ((header) & ~0x3)
#define CHUNK_PF(header) ((header)&M_PREV_FREE)
#define CHUNK_A(header) ((header)&M_ALLOCATED)

#define PACK(size, flags) (((size) & ~0x3) | (flags))

#define SW(p, w) (*((u32_t*)(p)) = w)
#define LW(p) (*((u32_t*)(p)))

#define HPTR(bp) ((u32_t*)(bp)-1)
#define BPTR(bp) ((uint8_t*)(bp) + WSIZE)
#define FPTR(hp, size) ((u32_t*)(hp + size - WSIZE))
#define NEXT_CHK(hp) ((uint8_t*)(hp) + CHUNK_S(LW(hp)))

#define BOUNDARY 4
#define WSIZE 4

#define HEAP_INIT_SIZE 4096

int
dmm_init(heap_context_t* heap);

int
lxbrk(heap_context_t* heap, void* addr, int user);

void*
lxsbrk(heap_context_t* heap, size_t size, int user);

void*
lx_malloc_internal(heap_context_t* heap, size_t size);

void
lx_free_internal(void* ptr);

#endif /* __LUNAIX_DMM_H */
