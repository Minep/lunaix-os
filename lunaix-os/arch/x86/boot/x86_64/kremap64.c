#define __BOOT_CODE__

#include <lunaix/mm/pagetable.h>
#include <lunaix/compiler.h>
#include <lunaix/sections.h>

#include <sys/boot/bstage.h>
#include <sys/mm/mm_defs.h>

#define RSVD_PAGES 32

#define ksection_maps autogen_name(ksecmap)
#define PF_X 0x1
#define PF_W 0x2

extern_autogen(ksecmap);

bridge_farsym(__kexec_text_start);
bridge_farsym(ksection_maps);

// define the initial page table layout
struct kernel_map;

static struct kernel_map kpt __section(".kpg");
export_symbol(debug, boot, kpt);

struct kernel_map 
{
    pte_t l0t[_PAGE_LEVEL_SIZE];        // root table
    pte_t l1t_rsvd[_PAGE_LEVEL_SIZE];   // 0~4G reservation

    struct {
        pte_t _lft[_PAGE_LEVEL_SIZE];
    } krsvd[RSVD_PAGES];
} align(8);

struct allocator
{
    struct kernel_map* kpt_pa;
    int pt_usage;
};

static inline ptr_t
alloc_rsvd_page(struct allocator* _alloc)
{
    if (_alloc->pt_usage >= KEXEC_RSVD) {
        asm ("ud2");
    }

    return __ptr(&_alloc->kpt_pa->krsvd[_alloc->pt_usage++]);
}

static pte_t* boot_text
prealloc_pt(struct allocator* _allc, ptr_t va, 
            pte_attr_t prot, size_t to_gran) 
{
    int lvl_i;
    pte_t *ptep, pte;
    size_t gran = L0T_SIZE;

    ptep = (pte_t*)&_allc->kpt_pa->l0t[0];

    for (int i = 0; i < _PTW_LEVEL && gran > to_gran; i++)
    {
        lvl_i = va_level_index(va, gran);
        ptep = &ptep[lvl_i];
        pte  = pte_at(ptep);

        gran = gran >> _PAGE_LEVEL_SHIFT;

        if (pte_isnull(pte)) {
            pte = mkpte(alloc_rsvd_page(_allc), KERNEL_PGTAB);
            if (to_gran == gran) {
                pte = pte_setprot(pte, prot);
            }

            set_pte(ptep, pte);
        }
        ptep = (pte_t*) pte_paddr(pte);
    }

    return ptep;
}

static void boot_text
do_remap()
{
    struct kernel_map* kpt_pa;
    pte_t *boot_l0tep, *klptep, *l1_rsvd;
    pte_t id_map, pte;
    ptr_t kstart;

    unsigned int lvl_i = 0;

    // identity map the first 4G for legacy compatibility
    kpt_pa     = (struct kernel_map*)to_kphysical(&kpt);
    boot_l0tep = (pte_t*) kpt_pa;
    l1_rsvd    = (pte_t*) kpt_pa->l1t_rsvd;
    id_map     = pte_mkhuge(mkpte_prot(KERNEL_PGTAB));
    
    pte = mkpte((ptr_t)l1_rsvd, KERNEL_PGTAB);
    set_pte(boot_l0tep, pte);
    
    for (int i = 0; i < 4; i++, l1_rsvd++)
    {
        id_map = pte_setpaddr(id_map, (ptr_t)i << 30);
        set_pte(l1_rsvd, id_map);
    }

    // Remap the kernel to -2GiB

    struct allocator alloc = {
        .kpt_pa = kpt_pa,
        .pt_usage = 0
    };

    prealloc_pt(&alloc, VMAP, KERNEL_PGTAB, L1T_SIZE);
    prealloc_pt(&alloc, PG_MOUNT_1, KERNEL_PGTAB, LFT_SIZE);

    kstart = page_aligned(__far(__kexec_text_start));

#if LnT_ENABLED(3)
    size_t gran  = L3T_SIZE;
#else
    size_t gran  = L2T_SIZE;
#endif

    prealloc_pt(&alloc, PMAP, KERNEL_PGTAB, gran);
    klptep = prealloc_pt(&alloc, kstart, KERNEL_PGTAB, gran);
    klptep += va_level_index(kstart, gran);

    pte = mkpte(0, KERNEL_PGTAB);
    for (int i = alloc.pt_usage; i < KEXEC_RSVD; i++)
    {
        pte = pte_setpaddr(pte, (ptr_t)&kpt_pa->krsvd[i]);
        set_pte(klptep++, pte);
    }

    struct ksecmap* maps;
    struct ksection* section;
    pfn_t pgs;

    maps = (struct ksecmap*)to_kphysical(__far(ksection_maps));

    // this is the first LFT we hooked on.
    // all these LFT are contig in physical address 
    klptep = (pte_t*) &kpt_pa->krsvd[alloc.pt_usage];
    klptep += pfn(to_kphysical(kstart));

    // Ensure the size of kernel is within the reservation
    int remain = KEXEC_RSVD - alloc.pt_usage;
    if (leaf_count(maps->ksize) > remain * _PAGE_LEVEL_SIZE) 
    {
        asm("ud2");
    }

    // assume contig kernel vaddrs
    for (unsigned int i = 0; i < maps->num; i++)
    {
        section = &maps->secs[i];

        if (section->va < KERNEL_RESIDENT) {
            continue;
        }

        pte = mkpte_prot(KERNEL_RDONLY);
        if ((section->flags & PF_X)) {
            pte = pte_mkexec(pte);
        }
        if ((section->flags & PF_W)) {
            pte = pte_mkwritable(pte);
        }

        pgs = leaf_count(section->size);
        for (pfn_t j = 0; j < pgs; j++)
        {
            pte = pte_setpaddr(pte, section->pa + page_addr(j));
            set_pte(klptep, pte);

            klptep++;
        }
    }

    // Build up self-reference
    lvl_i = va_level_index(VMS_SELF, L0T_SIZE);
    pte   = mkpte_root(__ptr(kpt_pa), KERNEL_PGTAB);
    set_pte(boot_l0tep + lvl_i, pte);
}


ptr_t boot_text
remap_kernel()
{
    ptr_t kmap_pa = to_kphysical(&kpt);
    
    asm volatile("movq %1, %%rdi\n"
                 "rep stosb\n" ::"c"(sizeof(kpt)),
                 "r"(kmap_pa),
                 "a"(0)
                 : "rdi", "memory");

    do_remap();

    return kmap_pa;
}