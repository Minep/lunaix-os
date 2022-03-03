/**
 * @file dmm.c
 * @author Lunaixsky
 * @brief Dynamic memory manager dedicated to kernel heap. Using implicit free
 * list implementation. This is designed to be portable, so it can serve as
 * syscalls to malloc/free in the c std lib. 
 * 
 * This version of code is however the simplest and yet insecured, 
 * it just to demonstrate how the malloc/free works behind the stage
 * 
 * @version 0.2
 * @date 2022-03-3
 *
 * @copyright Copyright (c) Lunaixsky 2022
 *
 */

#include <lunaix/mm/dmm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>

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

void*
coalesce(uint8_t* chunk_ptr);

void*
lx_grow_heap(heap_context_t* heap, size_t sz);

void
place_chunk(uint8_t* ptr, size_t size);

int
dmm_init(heap_context_t* heap)
{
    assert((uintptr_t)heap->start % BOUNDARY == 0);

    heap->brk = heap->start;

    vmm_alloc_page(heap->brk, PG_PREM_RW);

    SW(heap->start, PACK(4, M_ALLOCATED));
    SW(heap->start + WSIZE, PACK(0, M_ALLOCATED));
    heap->brk += WSIZE;

    return lx_grow_heap(heap, HEAP_INIT_SIZE) != NULL;
}

int
lxsbrk(heap_context_t* heap, void* addr)
{
    return lxbrk(heap, addr - heap->brk) != NULL;
}

void*
lxbrk(heap_context_t* heap, size_t size)
{
    if (size == 0) {
        return heap->brk;
    }

    // The upper bound of our next brk of heap given the size.
    // This will be used to calculate the page we need to allocate.
    // The "+ WSIZE" capture the overhead for epilogue marker
    void* next = heap->brk + ROUNDUP(size + WSIZE, WSIZE);

    if ((uintptr_t)next >= K_STACK_START) {
        return NULL;
    }

    uintptr_t heap_top_pg = PG_ALIGN(heap->brk);
    if (heap_top_pg != PG_ALIGN(next)) {
        // if next do require new pages to be allocated
        if (!vmm_alloc_pages((void*)(heap_top_pg + PG_SIZE),
                             ROUNDUP(size, PG_SIZE),
                             PG_PREM_RW)) {
            return NULL;
        }
    }

    void* old = heap->brk;
    heap->brk += size;
    return old;
}

void*
lx_grow_heap(heap_context_t* heap, size_t sz)
{
    void* start;

    if (!(start = lxbrk(heap, sz))) {
        return NULL;
    }
    sz = ROUNDUP(sz, BOUNDARY);

    uint32_t old_marker = *((uint32_t*)start);
    uint32_t free_hdr = PACK(sz, CHUNK_PF(old_marker));
    SW(start, free_hdr);
    SW(FPTR(start, sz), free_hdr);
    SW(NEXT_CHK(start), PACK(0, M_ALLOCATED | M_PREV_FREE));

    return coalesce(start);
}

void*
lx_malloc(heap_context_t* heap, size_t size)
{
    // Simplest first fit approach.

    uint8_t* ptr = heap->start;
    // round to largest 4B aligned value
    //  and space for header
    size = ROUNDUP(size, BOUNDARY) + WSIZE;
    while (ptr < (uint8_t*)heap->brk) {
        uint32_t header = *((uint32_t*)ptr);
        size_t chunk_size = CHUNK_S(header);
        if (chunk_size >= size && !CHUNK_A(header)) {
            // found!
            place_chunk(ptr, size);
            return BPTR(ptr);
        }
        ptr += chunk_size;
    }

    // if heap is full (seems to be!), then allocate more space (if it's
    // okay...)
    if ((ptr = lx_grow_heap(heap, size))) {
        place_chunk(ptr, size);
        return BPTR(ptr);
    }

    // Well, we are officially OOM!
    return NULL;
}

void
place_chunk(uint8_t* ptr, size_t size)
{
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
        uint32_t remainder_hdr = PACK(diff, M_NOT_ALLOCATED | M_PREV_ALLOCATED);
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
    size_t sz = CHUNK_S(hdr);
    uint8_t* next_hdr = chunk_ptr + sz;

    // make sure the ptr we are 'bout to free makes sense
    //   the size trick comes from:
    //  https://sourceware.org/git/?p=glibc.git;a=blob;f=malloc/malloc.c;h=1a1ac1d8f05b6f9bf295d7fdd0f12c2e4650a33c;hb=HEAD#l4437
    assert_msg(((uintptr_t)ptr < (uintptr_t)(-sz)) && !((uintptr_t)ptr & ~0x3),
               "free(): invalid pointer");
    assert_msg(sz > WSIZE && (sz & ~0x3),
               "free(): invalid size");

    SW(chunk_ptr, hdr & ~M_ALLOCATED);
    SW(FPTR(chunk_ptr, sz), hdr & ~M_ALLOCATED);
    SW(next_hdr, LW(next_hdr) | M_PREV_FREE);

    coalesce(chunk_ptr);
}

void*
coalesce(uint8_t* chunk_ptr)
{
    uint32_t hdr = LW(chunk_ptr);
    uint32_t pf = CHUNK_PF(hdr);
    uint32_t sz = CHUNK_S(hdr);

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