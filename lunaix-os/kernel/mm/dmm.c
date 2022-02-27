#include <lunaix/mm/dmm.h>
#include <lunaix/mm/vmm.h>

// This is a temporary design. 
//  We can do better when we are ready for multitasking
void
lxsbrk(void* current, void* next) {
    // TODO: sbrk 
}

void
lxmalloc(size_t size) {
    // TODO: Malloc 
}

void
lxfree(size_t size) {
    // TODO: Free 
}