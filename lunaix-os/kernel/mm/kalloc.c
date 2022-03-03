#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/dmm.h>

#include <libc/string.h>

#include <stdint.h>

extern uint8_t __kernel_heap_start;

heap_context_t __kalloc_kheap;

int
kalloc_init() {
    __kalloc_kheap.start = &__kernel_heap_start;
    __kalloc_kheap.brk = 0;

    return dmm_init(&__kalloc_kheap);
}

void*
kmalloc(size_t size) {
    return lx_malloc(&__kalloc_kheap, size);
}

void*
kcalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr) {
        return NULL;
    }

    return memset(ptr, 0, size);
}

void
kfree(void* ptr) {
    lx_free(ptr);
}