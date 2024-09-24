#include <lunaix/mm/procvm.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/page.h>

_default void
procvm_link_kernel(ptr_t dest_mnt)
{
    pte_t *ptep_smx, *src_smx;
    struct leaflet* leaflet;
    unsigned int i;
    
    i = va_level_index(KERNEL_RESIDENT, L0T_SIZE);
    ptep_smx = mkl1tep_va(VMS_SELF, dest_mnt);
    src_smx  = mkl0tep_va(VMS_SELF, 0);

    for (; i < LEVEL_SIZE; i++)
    {
        pte_t* ptep = &ptep_smx[i];
        pte_t  pte  = pte_at(&src_smx[i]);
        if (lntep_implie_vmnts(ptep, L0T_SIZE)) {
            continue;
        }

        // sanity check
        leaflet = pte_leaflet_aligned(pte);
        assert(leaflet_refcount(leaflet) > 0);

        set_pte(ptep, pte);
    }
}

_default void
procvm_unlink_kernel()
{
    // nothing to do here.
}