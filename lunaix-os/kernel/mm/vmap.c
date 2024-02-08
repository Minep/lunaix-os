#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#include <sys/mm/mempart.h>

static ptr_t start = VMAP;

void*
vmap(ptr_t paddr, size_t size, pt_attr attr, int flags)
{
    // next fit
    assert_msg((paddr & 0xfff) == 0, "vmap: bad alignment");
    size = ROUNDUP(size, PG_SIZE);

    ptr_t current_addr = start;
    size_t examed_size = 0, wrapped = 0;
    x86_page_table* pd = (x86_page_table*)L1_BASE_VADDR;

    while (!wrapped || current_addr < start) {
        size_t l1inx = L1_INDEX(current_addr);
        if (!(pd->entry[l1inx])) {
            // empty 4mb region
            examed_size += MEM_4M;
            current_addr = (current_addr & 0xffc00000) + MEM_4M;
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
            current_addr = VMAP;
        }
    }

    return NULL;

done:
    ptr_t alloc_begin = current_addr - examed_size;
    start = alloc_begin + size;

    // FIXME use latest vmm api
    if ((flags & VMAP_NOMAP)) {
        for (size_t i = 0; i < size; i += PG_SIZE) {
            vmm_set_mapping(VMS_SELF, alloc_begin + i, -1, 0);
        }

        return (void*)alloc_begin;
    }

    // FIXME use latest vmm api
    for (size_t i = 0; i < size; i += PG_SIZE) {
        vmm_set_mapping(VMS_SELF, alloc_begin + i, paddr + i, attr);
        pmm_ref_page(paddr + i);
    }

    return (void*)alloc_begin;
}

/*
    This is a kernel memory region that represent a contiguous virtual memory
   address such that all memory allocation/deallocation can be concentrated
   into a single big chunk, which will help to mitigate the external
   fragmentation in the VMAP address domain. It is significant if our
   allocation granule is single page or in some use cases.

    XXX (vmap_area)
    A potential performance improvement on pcache? (need more analysis!)
        -> In exchange of a fixed size buffer pool. (does it worth?)
*/

struct vmap_area*
vmap_varea(size_t size, pt_attr attr)
{
    ptr_t start = (ptr_t)vmap(0, size, attr ^ PG_PRESENT, VMAP_NOMAP);

    if (!start) {
        return NULL;
    }

    struct vmap_area* varea = valloc(sizeof(struct vmap_area));
    *varea =
      (struct vmap_area){ .start = start, .size = size, .area_attr = attr };

    return varea;
}

ptr_t
vmap_area_page(struct vmap_area* area, ptr_t paddr, pt_attr attr)
{
    ptr_t current = area->start;
    size_t bound = current + area->size;

    while (current < bound) {
        x86_pte_t* pte =
          (x86_pte_t*)(L2_VADDR(L1_INDEX(current)) | L2_INDEX(current));
        if (PG_IS_PRESENT(*pte)) {
            current += PG_SIZE;
            continue;
        }

        *pte = NEW_L2_ENTRY(attr | PG_PRESENT, paddr);
        cpu_flush_page(current);
        break;
    }

    return current;
}

ptr_t
vmap_area_rmpage(struct vmap_area* area, ptr_t vaddr)
{
    ptr_t current = area->start;
    size_t bound = current + area->size;

    if (current > vaddr || vaddr > bound) {
        return 0;
    }

    x86_pte_t* pte =
      (x86_pte_t*)(L2_VADDR(L1_INDEX(current)) | L2_INDEX(current));
    ptr_t pa = PG_ENTRY_ADDR(*pte);

    *pte = NEW_L2_ENTRY(0, -1);
    cpu_flush_page(current);

    return pa;
}