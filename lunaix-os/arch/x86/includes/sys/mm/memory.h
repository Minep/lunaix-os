#ifndef __LUNAIX_ARCH_MEMORY_H
#define __LUNAIX_ARCH_MEMORY_H

#include <lunaix/mm/pagetable.h>
#include <lunaix/mann_flags.h>

static inline pte_t
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
        pte = pte_mknonexec(pte);
    }

    return pte;
}


#endif /* __LUNAIX_ARCH_MEMORY_H */
