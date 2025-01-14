#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/page.h>

_default struct leaflet*
dup_leaflet(struct leaflet* leaflet)
{
    fail("unimplemented");
}

_default void
pmm_arch_init_pool(struct pmem* memory)
{
    pmm_declare_pool(POOL_UNIFIED, 1, memory->list_len);
}

_default ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx)
{
    fail("unimplemented");
}

_default pte_t
translate_vmr_prot(unsigned int vmr_prot, pte_t pte)
{
    pte = pte_mkuser(pte);

    if ((vmr_prot & PROT_WRITE)) {
        pte = pte_mkwritable(pte);
    }

    if ((vmr_prot & PROT_EXEC)) {
        pte = pte_mkexec(pte);
    }
    else {
        pte = pte_mknexec(pte);
    }

    return pte;
}
