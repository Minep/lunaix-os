
/**** DO NOT USE ****/

/**
 * @file kalloc.c
 * @author Lunaixsky
 * @brief Implicit free list implementation of malloc family, for kernel use.
 *
 * This version of code is however the simplest and yet insecured, thread unsafe
 * it just to demonstrate how the malloc/free works behind the curtain
 * @version 0.1
 * @date 2022-03-05
 *
 * @copyright Copyright (c) 2022
 *
 */
// #include <lunaix/mm/dmm.h>
// #include <lunaix/mm/kalloc.h>
// #include <lunaix/mm/vmm.h>

// #include <lunaix/common.h>
// #include <lunaix/spike.h>

// #include <klibc/string.h>

// #include <stdint.h>

// extern uint8_t __kernel_heap_start;

// void*
// lx_malloc_internal(heap_context_t* heap, size_t size);

// void
// place_chunk(uint8_t* ptr, size_t size);

// void
// lx_free_internal(void* ptr);

// void*
// coalesce(uint8_t* chunk_ptr);

// void*
// lx_grow_heap(heap_context_t* heap, size_t sz);

// /*
//     At the beginning, we allocate an empty page and put our initial marker

//     | 4/1 | 0/1 |
//     ^     ^ brk
//     start

//     Then, expand the heap further, with HEAP_INIT_SIZE (evaluated to 4096,
//     i.e.,
//    1 pg size) This will allocate as much pages and override old epilogue
//    marker with a free region hdr and put new epilogue marker. These are
//    handled by lx_grow_heap which is internally used by alloc to expand the
//    heap at many moment when needed.

//     | 4/1 | 4096/0 |   .......   | 4096/0 | 0/1 |
//     ^     ^ brk_old                       ^
//     start                                 brk

//     Note: the brk always point to the beginning of epilogue.
// */

// static heap_context_t kheap;

// int
// kalloc_init()
// {
//     kheap.start = KHEAP_START;
//     kheap.brk = NULL;
//     kheap.max_addr =
//       (void*)PROC_START; // 在新的布局中，堆结束的地方即为进程表开始的地方

//     for (size_t i = 0; i < KHEAP_SIZE_MB >> 2; i++) {
//         vmm_set_mapping(PD_REFERENCED,
//                         (uintptr_t)kheap.start + (i << 22),
//                         0,
//                         PG_PREM_RW,
//                         VMAP_NOMAP);
//     }

//     if (!dmm_init(&kheap)) {
//         return 0;
//     }

//     SW(kheap.start, PACK(4, M_ALLOCATED));
//     SW(kheap.start + WSIZE, PACK(0, M_ALLOCATED));
//     kheap.brk += WSIZE;

//     return lx_grow_heap(&kheap, HEAP_INIT_SIZE) != NULL;
// }

// void*
// lxmalloc(size_t size)
// {
//     mutex_lock(&kheap.lock);
//     void* r = lx_malloc_internal(&kheap, size);
//     mutex_unlock(&kheap.lock);

//     return r;
// }

// void*
// lxcalloc(size_t n, size_t elem)
// {
//     size_t pd = n * elem;

//     // overflow detection
//     if (pd < elem || pd < n) {
//         return NULL;
//     }

//     void* ptr = lxmalloc(pd);
//     if (!ptr) {
//         return NULL;
//     }

//     return memset(ptr, 0, pd);
// }

// void
// lxfree(void* ptr)
// {
//     if (!ptr) {
//         return;
//     }
//     mutex_lock(&kheap.lock);

//     uint8_t* chunk_ptr = (uint8_t*)ptr - WSIZE;
//     u32_t hdr = LW(chunk_ptr);
//     size_t sz = CHUNK_S(hdr);
//     uint8_t* next_hdr = chunk_ptr + sz;

//     // make sure the ptr we are 'bout to free makes sense
//     //   the size trick is stolen from glibc's malloc/malloc.c:4437 ;P

//     assert_msg(((uintptr_t)ptr < (uintptr_t)(-sz)) && !((uintptr_t)ptr &
//     0x3),
//                "free(): invalid pointer");

//     assert_msg(sz > WSIZE, "free(): invalid size");

//     SW(chunk_ptr, hdr & ~M_ALLOCATED);
//     SW(FPTR(chunk_ptr, sz), hdr & ~M_ALLOCATED);
//     SW(next_hdr, LW(next_hdr) | M_PREV_FREE);

//     coalesce(chunk_ptr);

//     mutex_unlock(&kheap.lock);
// }

// void*
// lx_malloc_internal(heap_context_t* heap, size_t size)
// {
//     // Simplest first fit approach.

//     if (!size) {
//         return NULL;
//     }

//     uint8_t* ptr = heap->start;
//     // round to largest 4B aligned value
//     //  and space for header
//     size = ROUNDUP(size + WSIZE, BOUNDARY);
//     while (ptr < (uint8_t*)heap->brk) {
//         u32_t header = *((u32_t*)ptr);
//         size_t chunk_size = CHUNK_S(header);
//         if (!chunk_size && CHUNK_A(header)) {
//             break;
//         }
//         if (chunk_size >= size && !CHUNK_A(header)) {
//             // found!
//             place_chunk(ptr, size);
//             return BPTR(ptr);
//         }
//         ptr += chunk_size;
//     }

//     // if heap is full (seems to be!), then allocate more space (if it's
//     // okay...)
//     if ((ptr = lx_grow_heap(heap, size))) {
//         place_chunk(ptr, size);
//         return BPTR(ptr);
//     }

//     // Well, we are officially OOM!
//     return NULL;
// }

// void
// place_chunk(uint8_t* ptr, size_t size)
// {
//     u32_t header = *((u32_t*)ptr);
//     size_t chunk_size = CHUNK_S(header);
//     *((u32_t*)ptr) = PACK(size, CHUNK_PF(header) | M_ALLOCATED);
//     uint8_t* n_hdrptr = (uint8_t*)(ptr + size);
//     u32_t diff = chunk_size - size;

//     if (!diff) {
//         // if the current free block is fully occupied
//         u32_t n_hdr = LW(n_hdrptr);
//         // notify the next block about our avaliability
//         SW(n_hdrptr, n_hdr & ~0x2);
//     } else {
//         // if there is remaining free space left
//         u32_t remainder_hdr = PACK(diff, M_NOT_ALLOCATED |
//         M_PREV_ALLOCATED); SW(n_hdrptr, remainder_hdr); SW(FPTR(n_hdrptr,
//         diff), remainder_hdr);

//         /*
//             | xxxx |      |         |

//                         |
//                         v

//             | xxxx |                |
//         */
//         coalesce(n_hdrptr);
//     }
// }

// void*
// coalesce(uint8_t* chunk_ptr)
// {
//     u32_t hdr = LW(chunk_ptr);
//     u32_t pf = CHUNK_PF(hdr);
//     u32_t sz = CHUNK_S(hdr);

//     u32_t n_hdr = LW(chunk_ptr + sz);

//     if (CHUNK_A(n_hdr) && pf) {
//         // case 1: prev is free
//         u32_t prev_ftr = LW(chunk_ptr - WSIZE);
//         size_t prev_chunk_sz = CHUNK_S(prev_ftr);
//         u32_t new_hdr = PACK(prev_chunk_sz + sz, CHUNK_PF(prev_ftr));
//         SW(chunk_ptr - prev_chunk_sz, new_hdr);
//         SW(FPTR(chunk_ptr, sz), new_hdr);
//         chunk_ptr -= prev_chunk_sz;
//     } else if (!CHUNK_A(n_hdr) && !pf) {
//         // case 2: next is free
//         size_t next_chunk_sz = CHUNK_S(n_hdr);
//         u32_t new_hdr = PACK(next_chunk_sz + sz, pf);
//         SW(chunk_ptr, new_hdr);
//         SW(FPTR(chunk_ptr, sz + next_chunk_sz), new_hdr);
//     } else if (!CHUNK_A(n_hdr) && pf) {
//         // case 3: both free
//         u32_t prev_ftr = LW(chunk_ptr - WSIZE);
//         size_t next_chunk_sz = CHUNK_S(n_hdr);
//         size_t prev_chunk_sz = CHUNK_S(prev_ftr);
//         u32_t new_hdr =
//           PACK(next_chunk_sz + prev_chunk_sz + sz, CHUNK_PF(prev_ftr));
//         SW(chunk_ptr - prev_chunk_sz, new_hdr);
//         SW(FPTR(chunk_ptr, sz + next_chunk_sz), new_hdr);
//         chunk_ptr -= prev_chunk_sz;
//     }

//     // (fall through) case 4: prev and next are not free
//     return chunk_ptr;
// }

// void*
// lx_grow_heap(heap_context_t* heap, size_t sz)
// {
//     void* start;

//     // The "+ WSIZE" capture the overhead for epilogue marker
//     if (!(start = lxsbrk(heap, sz + WSIZE, 0))) {
//         return NULL;
//     }
//     sz = ROUNDUP(sz, BOUNDARY);

//     // minus the overhead for epilogue, keep the invariant.
//     heap->brk -= WSIZE;

//     u32_t old_marker = *((u32_t*)start);
//     u32_t free_hdr = PACK(sz, CHUNK_PF(old_marker));
//     SW(start, free_hdr);
//     SW(FPTR(start, sz), free_hdr);
//     SW(NEXT_CHK(start), PACK(0, M_ALLOCATED | M_PREV_FREE));

//     return coalesce(start);
// }