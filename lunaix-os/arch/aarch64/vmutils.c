#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/page.h>

struct leaflet*
dup_leaflet(struct leaflet* leaflet)
{
    ptr_t dest_va, src_va;
    struct leaflet* new_leaflet;
    
    new_leaflet = alloc_leaflet(leaflet_order(leaflet));

    src_va = leaflet_mount(leaflet);
    dest_va = vmap(new_leaflet, KERNEL_DATA);

    memcpy((void*)dest_va, (void*)src_va, PAGE_SIZE);

    leaflet_unmount(leaflet);
    vunmap(dest_va, new_leaflet);

    return new_leaflet;
}

ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx)
{
    unsigned long plist_len;

    plist_len = leaf_count(bctx->mem.size) * sizeof(struct ppage);

    for (int i = 0; i < bctx->mem.mmap_len; i++) {
        
    }
}
