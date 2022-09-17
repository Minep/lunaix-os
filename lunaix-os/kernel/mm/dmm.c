/**
 * @file dmm.c
 * @author Lunaixsky
 * @brief Dynamic memory manager for heap. This design do not incorporate any\
 * specific implementation of malloc family. The main purpose of this routines
 * is to provide handy method to initialize & grow the heap as needed by
 * upstream implementation.
 *
 * This is designed to be portable, so it can serve as syscalls to malloc/free
 * in the c std lib.
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
#include <lunaix/status.h>

#include <lunaix/spike.h>
#include <lunaix/syscall.h>

__DEFINE_LXSYSCALL1(int, sbrk, size_t, size)
{
    heap_context_t* uheap = &__current->mm.u_heap;
    mutex_lock(&uheap->lock);
    void* r = lxsbrk(uheap, size, PG_ALLOW_USER);
    mutex_unlock(&uheap->lock);
    return r;
}

__DEFINE_LXSYSCALL1(void*, brk, void*, addr)
{
    heap_context_t* uheap = &__current->mm.u_heap;
    mutex_lock(&uheap->lock);
    int r = lxbrk(uheap, addr, PG_ALLOW_USER);
    mutex_unlock(&uheap->lock);
    return r;
}

int
dmm_init(heap_context_t* heap)
{
    assert((uintptr_t)heap->start % BOUNDARY == 0);

    heap->brk = heap->start;
    mutex_init(&heap->lock);

    return vmm_set_mapping(PD_REFERENCED,
                           heap->brk,
                           0,
                           PG_WRITE | PG_ALLOW_USER,
                           VMAP_NULL) != NULL;
}

int
lxbrk(heap_context_t* heap, void* addr, int user)
{
    return -(lxsbrk(heap, addr - heap->brk, user) == (void*)-1);
}

void*
lxsbrk(heap_context_t* heap, size_t size, int user)
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
        __current->k_status = LXINVLDPTR;
        return (void*)-1;
    }

    uintptr_t diff = PG_ALIGN(next) - PG_ALIGN(current_brk);
    if (diff) {
        // if next do require new pages to be mapped
        for (size_t i = 0; i < diff; i += PG_SIZE) {
            vmm_set_mapping(PD_REFERENCED,
                            PG_ALIGN(current_brk) + PG_SIZE + i,
                            0,
                            PG_WRITE | user,
                            VMAP_NULL);
        }
    }

    heap->brk += size;
    return current_brk;
}