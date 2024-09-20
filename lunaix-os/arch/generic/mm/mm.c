#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/page.h>

struct leaflet*
dup_leaflet(struct leaflet* leaflet)
{
    fail("unimplemented");
}

void
pmm_arch_init_pool(struct pmem* memory)
{
    fail("unimplemented");
}

ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx)
{
    fail("unimplemented");
}
