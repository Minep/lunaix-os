/**
 * @file dmm.c
 * @author Lunaixsky
 * @brief Dynamic memory manager dedicated to kernel heap. It is not portable at
 * this moment. Implicit free list implementation.
 * @version 0.1
 * @date 2022-02-28
 *
 * @copyright Copyright (c) Lunaixsky 2022
 *
 */

#include <lunaix/mm/dmm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>

#include <lunaix/assert.h>
#include <lunaix/constants.h>
#include <lunaix/spike.h>

#define M_ALLOCATED 0x1
#define M_PREV_FREE 0x2

#define M_NOT_ALLOCATED 0x0
#define M_PREV_ALLOCATED 0x0

#define CHUNK_S(header) ((header) & ~0x3)
#define CHUNK_PF(header) ((header)&M_PREV_FREE)
#define CHUNK_A(header) ((header)&M_ALLOCATED)

#define PACK(size, flags) (((size) & ~0x3) | (flags))

#define SW(p, w) (*((uint32_t*)(p)) = w)
#define LW(p) (*((uint32_t*)(p)))

#define HPTR(bp) ((uint32_t*)(bp)-1)
#define BPTR(bp) ((uint8_t*)(bp) + WSIZE)
#define FPTR(hp, size) ((uint32_t*)(hp + size - WSIZE))
#define NEXT_CHK(hp) ((uint8_t*)(hp) + CHUNK_S(LW(hp)))

#define BOUNDARY 4
#define WSIZE 4

extern uint8_t __kernel_heap_start;

void* current_heap_top = NULL;

void*
coalesce(uint8_t* chunk_ptr);

void*
lx_grow_heap(size_t sz);

void place_chunk(uint8_t* ptr, size_t size);

int
dmm_init()
{
    assert((uintptr_t)&__kernel_heap_start % BOUNDARY == 0);

    current_heap_top = &__kernel_heap_start;
    uint8_t* heap_start = &__kernel_heap_start;
    
    vmm_alloc_page(current_heap_top, PG_PREM_RW);

    SW(heap_start,     PACK(4, M_ALLOCATED));
    SW(heap_start + WSIZE, PACK(0, M_ALLOCATED));
    current_heap_top += WSIZE;

    return lx_grow_heap(HEAP_INIT_SIZE);
}

int
lxsbrk(void* addr)
{
    return lxbrk(addr - current_heap_top) != NULL;
}

void*
lxbrk(size_t size)
{   
    if (size == 0) {
        return NULL;
    }

    // plus WSIZE is the overhead for epilogue marker
    size += WSIZE;
    void* next = current_heap_top + ROUNDUP((uintptr_t)size, WSIZE);

    if (next >= K_STACK_START) {
        return NULL;
    }

    // Check the invariant
    assert(size % BOUNDARY == 0)

    uintptr_t heap_top_pg = PG_ALIGN(current_heap_top);
      if (heap_top_pg != PG_ALIGN(next))
    {
        // if next do require new pages to be allocated
        if (!vmm_alloc_pages(heap_top_pg + PG_SIZE, ROUNDUP(size, PG_SIZE), PG_PRESENT | PG_WRITE)) {
            return NULL;
        }
    
    }

    uintptr_t old = current_heap_top;
    current_heap_top = next - WSIZE;
    return old;
}

void*
lx_grow_heap(size_t sz) {
    uintptr_t start;

    sz = ROUNDUP(sz, BOUNDARY);
    if (!(start = lxbrk(sz))) {
        return NULL;
    }

    uint32_t old_marker = *((uint32_t*)start);
    uint32_t free_hdr = PACK(sz, CHUNK_PF(old_marker));
    SW(start, free_hdr);
    SW(FPTR(start, sz), free_hdr);
    SW(NEXT_CHK(start), PACK(0, M_ALLOCATED | M_PREV_FREE));

    return coalesce(start);
}

void*
lx_malloc(size_t size)
{
    // Simplest first fit approach.

    uint8_t* ptr = &__kernel_heap_start;
    // round to largest 4B aligned value
    //  and space for header
    size = ROUNDUP(size, BOUNDARY) + WSIZE;
    while (ptr < current_heap_top) {
        uint32_t header = *((uint32_t*)ptr);
        size_t chunk_size = CHUNK_S(header);
        if (chunk_size >= size && !CHUNK_A(header)) {
            // found!
            place_chunk(ptr, size);
            return BPTR(ptr);
        }
        ptr += chunk_size;
    }

    // if heap is full (seems to be!), then allocate more space (if it's okay...)
    if ((ptr = lx_grow_heap(size))) {
        place_chunk(ptr, size);
        return BPTR(ptr);
    }

    // Well, we are officially OOM!
    return NULL;
}

void place_chunk(uint8_t* ptr, size_t size) {
    uint32_t header = *((uint32_t*)ptr);
    size_t chunk_size = CHUNK_S(header);
    *((uint32_t*)ptr) = PACK(size, CHUNK_PF(header) | M_ALLOCATED);
    uint8_t* n_hdrptr = (uint8_t*)(ptr + size);
    uint32_t diff = chunk_size - size;
    if (!diff) {
        // if the current free block is fully occupied
        uint32_t n_hdr = LW(n_hdrptr);
        // notify the next block about our avaliability
        SW(n_hdrptr, n_hdr & ~0x2);
    } else {
        // if there is remaining free space left
        uint32_t remainder_hdr =
            PACK(diff, M_NOT_ALLOCATED | M_PREV_ALLOCATED);
        SW(n_hdrptr, remainder_hdr);
        SW(FPTR(n_hdrptr, diff), remainder_hdr);

        coalesce(n_hdrptr);
    }
}

void
lx_free(void* ptr)
{
    if (!ptr) {
        return;
    }

    uint8_t* chunk_ptr = (uint8_t*)ptr - WSIZE;
    uint32_t hdr = LW(chunk_ptr);
    uint8_t* next_hdr = chunk_ptr + CHUNK_S(hdr);

    SW(chunk_ptr, hdr & ~M_ALLOCATED);
    SW(FPTR(chunk_ptr, CHUNK_S(hdr)), hdr & ~M_ALLOCATED);
    SW(next_hdr, LW(next_hdr) | M_PREV_FREE);

    coalesce(chunk_ptr);
}

void*
coalesce(uint8_t* chunk_ptr)
{
    uint32_t hdr = LW(chunk_ptr);
    uint32_t pf = CHUNK_PF(hdr);
    uint32_t sz = CHUNK_S(hdr);
    uint32_t ftr = LW(chunk_ptr + sz - WSIZE);

    uint32_t n_hdr = LW(chunk_ptr + sz);

    if (CHUNK_A(n_hdr) && pf) {
        // case 1: prev is free
        uint32_t prev_ftr = LW(chunk_ptr - WSIZE);
        size_t prev_chunk_sz = CHUNK_S(prev_ftr);
        uint32_t new_hdr = PACK(prev_chunk_sz + sz, CHUNK_PF(prev_ftr));
        SW(chunk_ptr - prev_chunk_sz, new_hdr);
        SW(FPTR(chunk_ptr, sz), new_hdr);
        chunk_ptr -= prev_chunk_sz;
    } else if (!CHUNK_A(n_hdr) && !pf) {
        // case 2: next is free
        size_t next_chunk_sz = CHUNK_S(n_hdr);
        uint32_t new_hdr = PACK(next_chunk_sz + sz, pf);
        SW(chunk_ptr, new_hdr);
        SW(FPTR(chunk_ptr, sz + next_chunk_sz), new_hdr);
    } else if (!CHUNK_A(n_hdr) && pf) {
        // case 3: both free
        uint32_t prev_ftr = LW(chunk_ptr - WSIZE);
        size_t next_chunk_sz = CHUNK_S(n_hdr);
        size_t prev_chunk_sz = CHUNK_S(prev_ftr);
        uint32_t new_hdr =
          PACK(next_chunk_sz + prev_chunk_sz + sz, CHUNK_PF(prev_ftr));
        SW(chunk_ptr - prev_chunk_sz, new_hdr);
        SW(FPTR(chunk_ptr, sz + next_chunk_sz), new_hdr);
        chunk_ptr -= prev_chunk_sz;
    }

    // case 4: prev and next are not free
    return chunk_ptr;
}