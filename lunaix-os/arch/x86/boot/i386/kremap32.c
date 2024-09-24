#define __BOOT_CODE__

#include <lunaix/mm/pagetable.h>
#include <lunaix/compiler.h>
#include <lunaix/sections.h>

#include <sys/boot/bstage.h>
#include <asm/mm_defs.h>

#define PF_X 0x1
#define PF_W 0x2
#define ksection_maps autogen_name(ksecmap)

extern_autogen(ksecmap);

bridge_farsym(__kexec_text_start);
bridge_farsym(ksection_maps);

// define the initial page table layout
struct kernel_map;

static struct kernel_map kernel_pt __section(".kpg");
export_symbol(debug, boot, kernel_pt);

struct kernel_map 
{
    pte_t l0t[_PAGE_LEVEL_SIZE];
    pte_t pg_mnt[_PAGE_LEVEL_SIZE];

    struct {
        pte_t _lft[_PAGE_LEVEL_SIZE];
    } kernel_lfts[16];
} align(4);

static void boot_text
do_remap()
{
    struct kernel_map* kpt_pa = (struct kernel_map*)to_kphysical(&kernel_pt);
    
    size_t mia_casa_i   = pfn_at(KERNEL_RESIDENT, L0T_SIZE);
    pte_t* klptep       = (pte_t*) &kpt_pa->l0t[mia_casa_i];
    pte_t* ktep         = (pte_t*)  kpt_pa->kernel_lfts;    
    pte_t* boot_l0tep   = (pte_t*)  kpt_pa;

    set_pte(boot_l0tep, pte_mkhuge(mkpte_prot(KERNEL_PGTAB)));

    // --- 将内核重映射至高半区 ---

    // Hook the kernel reserved LFTs onto L0T
    pte_t pte = mkpte((ptr_t)ktep, KERNEL_PGTAB);
    
    for (u32_t i = 0; i < KEXEC_RSVD; i++) {
        pte = pte_setpaddr(pte, (ptr_t)&kpt_pa->kernel_lfts[i]);
        set_pte(klptep, pte);

        klptep++;
    }

    struct ksecmap* maps;
    struct ksection* section;
    pfn_t pgs;
    pte_t *kmntep;

    maps = (struct ksecmap*)to_kphysical(__far(ksection_maps));
    ktep += pfn(to_kphysical(__far(__kexec_text_start)));

    // Ensure the size of kernel is within the reservation
    if (leaf_count(maps->ksize) > KEXEC_RSVD * _PAGE_LEVEL_SIZE) 
    {
        // ERROR: require more pages
        //  here should do something else other than head into blocking
        asm("ud2");
    }

    // Now, map the sections

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
            set_pte(ktep, pte);

            ktep++;
        }
    }

    // set mount point
    kmntep = (pte_t*) &kpt_pa->l0t[pfn_at(PG_MOUNT_1, L0T_SIZE)];
    set_pte(kmntep, mkpte((ptr_t)kpt_pa->pg_mnt, KERNEL_PGTAB));

    // Build up self-reference
    int level = (VMS_SELF / L0T_SIZE) & _PAGE_LEVEL_MASK;
    
    pte = mkpte_root((ptr_t)kpt_pa, KERNEL_PGTAB);
    set_pte(&boot_l0tep[level], pte);
}

ptr_t boot_text
remap_kernel()
{
    ptr_t kmap_pa = to_kphysical(&kernel_pt);
    for (size_t i = 0; i < sizeof(kernel_pt); i++) {
        ((u8_t*)kmap_pa)[i] = 0;
    }

    do_remap();

    return kmap_pa;
}