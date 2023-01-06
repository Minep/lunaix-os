#include <lunaix/mm/mmap.h>
#include <lunaix/process.h>
#include <lunaix/status.h>

#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

__DEFINE_LXSYSCALL1(void*, sbrk, ssize_t, incr)
{
    struct proc_mm* pvms = &__current->mm;
    struct mm_region* heap = pvms->heap;

    assert(heap);
    int err = mem_adjust_inplace(&pvms->regions, heap, heap->end + incr);
    if (err) {
        return (void*)DO_STATUS(err);
    }
    return (void*)heap->end;
}

__DEFINE_LXSYSCALL1(int, brk, void*, addr)
{
    struct proc_mm* pvms = &__current->mm;
    struct mm_region* heap = pvms->heap;

    assert(heap);
    int err = mem_adjust_inplace(&pvms->regions, heap, (ptr_t)addr);
    return DO_STATUS(err);
}