#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#define VMAP_START PG_MOUNT_BASE + MEM_4MB
#define VMAP_END PD_REFERENCED

static uintptr_t start = VMAP_START;

void*
vmm_vmap(uintptr_t paddr, size_t size, pt_attr attr)
{
    // next fit
    assert_msg((paddr & 0xfff) == 0, "vmap: bad alignment");
    size = ROUNDUP(size, PG_SIZE);

    uintptr_t current_addr = start;
    size_t examed_size = 0, wrapped = 0;
    x86_page_table* pd = (x86_page_table*)L1_BASE_VADDR;

    while (!wrapped || current_addr >= start) {
        size_t l1inx = L1_INDEX(current_addr);
        if (!(pd->entry[l1inx])) {
            // empty 4mb region
            examed_size += MEM_4MB;
            current_addr = (current_addr & 0xffc00000) + MEM_4MB;
        } else {
            x86_page_table* ptd = (x86_page_table*)(L2_VADDR(l1inx));
            size_t i = L2_INDEX(current_addr), j = 0;
            for (; i < PG_MAX_ENTRIES && examed_size < size; i++, j++) {
                if (!ptd->entry[i]) {
                    examed_size += PG_SIZE;
                } else if (examed_size) {
                    // found a discontinuity, start from beginning
                    examed_size = 0;
                    j++;
                    break;
                }
            }
            current_addr += j << 12;
        }

        if (examed_size >= size) {
            goto done;
        }

        if (current_addr >= VMAP_END) {
            wrapped = 1;
            examed_size = 0;
            current_addr = VMAP_START;
        }
    }

    return NULL;

done:
    uintptr_t alloc_begin = current_addr - examed_size;
    for (size_t i = 0; i < size; i += PG_SIZE) {
        vmm_set_mapping(
          PD_REFERENCED, alloc_begin + i, paddr + i, PG_PREM_RW, 0);
        pmm_ref_page(KERNEL_PID, paddr + i);
    }
    start = alloc_begin + size;

    return (void*)alloc_begin;
}