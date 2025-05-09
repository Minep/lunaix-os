#include <lunaix/mm/page.h>

#include <asm/mm_defs.h>

struct leaflet*
dup_leaflet(struct leaflet* leaflet)
{
    ptr_t dest_va, src_va;
    struct leaflet* new_leaflet;
    
    new_leaflet = alloc_leaflet(leaflet_order(leaflet));

    // TODO need a proper long term fix for the contention of page mount point
    dest_va = vmap(new_leaflet, KERNEL_DATA);
    src_va = leaflet_mount(leaflet);

    size_t cnt_wordsz = leaflet_size(new_leaflet) / sizeof(ptr_t);

#ifdef CONFIG_ARCH_X86_64
    asm volatile("movq %1, %%rdi\n"
                 "movq %2, %%rsi\n"
                 "rep movsq\n" ::"c"(cnt_wordsz),
                 "r"(dest_va),
                 "r"(src_va)
                 : "memory", "%edi", "%esi");

#else
    asm volatile("movl %1, %%edi\n"
                 "movl %2, %%esi\n"
                 "rep movsl\n" ::"c"(cnt_wordsz),
                 "r"(dest_va),
                 "r"(src_va)
                 : "memory", "%edi", "%esi");

#endif

    leaflet_unmount(leaflet);
    vunmap(dest_va, new_leaflet);

    return new_leaflet;
}

pte_t
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
