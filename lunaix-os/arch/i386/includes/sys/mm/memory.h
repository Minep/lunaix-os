#ifndef __LUNAIX_MEMORY_H
#define __LUNAIX_MEMORY_H

#include <lunaix/mm/pagetable.h>
#include <lunaix/mann_flags.h>

static inline pte_attr_t
translate_vmr_prot(unsigned int vmr_prot)
{
    pte_attr_t _pte_prot = _PTE_U;
    if ((vmr_prot & PROT_READ)) {
        _pte_prot |= _PTE_R;
    }

    if ((vmr_prot & PROT_WRITE)) {
        _pte_prot |= _PTE_W;
    }

    if ((vmr_prot & PROT_EXEC)) {
        _pte_prot |= _PTE_X;
    }

    return _pte_prot;
}


#endif /* __LUNAIX_MEMORY_H */
