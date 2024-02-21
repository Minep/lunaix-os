#include <lunaix/mm/page.h>
#include <sys/mm/mm_defs.h>

struct leaflet*
dup_leaflet(struct leaflet* leaflet)
{
    ptr_t dest_va, src_va;
    struct leaflet* new_leaflet;
    
    new_leaflet = alloc_leaflet(leaflet_order(leaflet));

    src_va = leaflet_mount(leaflet);
    dest_va = vmap(new_leaflet, KERNEL_DATA);

    asm volatile("movl %1, %%edi\n"
                 "movl %2, %%esi\n"
                 "rep movsl\n" ::"c"(1024),
                 "r"(dest_va),
                 "r"(src_va)
                 : "memory", "%edi", "%esi");

    leaflet_unmount(leaflet);
    vunmap(dest_va, new_leaflet);

    return new_leaflet;
}