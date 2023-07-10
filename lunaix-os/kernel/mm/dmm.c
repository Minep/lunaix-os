#include <lunaix/mm/mmap.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/status.h>

#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

void
__heap_copied(struct mm_region* region)
{
    mm_index((void**)&region->proc_vms->heap, region);
}

int
create_heap(struct proc_mm* pvms, ptr_t addr)
{
    struct mmap_param map_param = { .pvms = pvms,
                                    .vms_mnt = VMS_SELF,
                                    .flags = MAP_ANON | MAP_PRIVATE,
                                    .type = REGION_TYPE_HEAP,
                                    .proct = PROT_READ | PROT_WRITE,
                                    .mlen = PG_SIZE };
    int status = 0;
    struct mm_region* heap;
    if ((status = mem_map(NULL, &heap, addr, NULL, &map_param))) {
        return status;
    }

    heap->region_copied = __heap_copied;
    mm_index((void**)&pvms->heap, heap);
}

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

    if (!heap) {
        return DO_STATUS(create_heap(pvms, addr));
    }

    assert(heap);
    int err = mem_adjust_inplace(&pvms->regions, heap, (ptr_t)addr);
    return DO_STATUS(err);
}