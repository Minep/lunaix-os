#include "asm/mempart.h"
#include <lunaix/mm/procvm.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/page.h>

_default void
procvm_link_kernel(pte_t* dest_l0t, pte_t* src_l0t)
{
    int index, end;

    index = level_index(KERNEL_AREA_BEGIN, L0T);
    end = level_index(KERNEL_AREA_END, L0T);
    
    for(; index <= end; index++) {
        set_pte(&dest_l0t[index], pte_at(&src_l0t[index]));
    }
}

_default void
procvm_unlink_kernel()
{
    // nothing to do here.
}
