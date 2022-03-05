/**
 * @file dmm.c
 * @author Lunaixsky
 * @brief Dynamic memory manager for heap. This design do not incorporate any\
 * specific implementation of malloc family. The main purpose of this routines is to
 * provide handy method to initialize & grow the heap as needed by upstream implementation.
 * 
 * This is designed to be portable, so it can serve as syscalls to malloc/free in the c std lib. 
 * 
 * @version 0.2
 * @date 2022-03-3
 *
 * @copyright Copyright (c) Lunaixsky 2022
 *
 */

#include <lunaix/mm/dmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/page.h>

#include <lunaix/spike.h>

int
dmm_init(heap_context_t* heap)
{
    assert((uintptr_t)heap->start % BOUNDARY == 0);

    heap->brk = heap->start;

    return vmm_alloc_page(heap->brk, PG_PREM_RW) != NULL;
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

    void* current_brk = heap->brk;

    // The upper bound of our next brk of heap given the size.
    // This will be used to calculate the page we need to allocate.
    void* next = current_brk + ROUNDUP(size, BOUNDARY);

    // any invalid situations
    if (next >= heap->max_addr || next < current_brk) {
        return NULL;
    }

    uintptr_t diff = PG_ALIGN(next) - PG_ALIGN(current_brk);
    if (diff) {
        // if next do require new pages to be allocated
        if (!vmm_alloc_pages((void*)(PG_ALIGN(current_brk) + PG_SIZE),
                             diff,
                             PG_PREM_RW)) {
            // for debugging
            assert_msg(0, "unable to brk");
            return NULL;
        }
    }

    heap->brk += size;
    return current_brk;
}