#define __BOOT_CODE__

#include <lunaix/mm/pagetable.h>
#include <lunaix/compiler.h>

#include <sys/boot/bstage.h>
#include <sys/mm/mm_defs.h>

bridge_farsym(__kexec_start);
bridge_farsym(__kexec_end);
bridge_farsym(__kexec_text_start);
bridge_farsym(__kexec_text_end);

// define the initial page table layout
struct kernel_map;

static struct kernel_map kernel_pt __section(".kpg");
export_symbol(debug, boot, kernel_pt);

struct kernel_map {
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
    
    size_t mio_casa_i   = pfn_at(KERNEL_RESIDENT, L0T_SIZE);
    pte_t* klptep       = (pte_t*) &kpt_pa->l0t[mio_casa_i];
    pte_t* ktep         = (pte_t*)  kpt_pa->kernel_lfts;    
    pte_t* boot_l0tep   = (pte_t*)  kpt_pa;

    set_pte(boot_l0tep, pte_mkhuge(mkpte_prot(KERNEL_DATA)));

    // --- 将内核重映射至高半区 ---

    // Hook the kernel reserved LFTs onto L0T
    pte_t pte = mkpte((ptr_t)ktep, KERNEL_DATA);
    
    for (u32_t i = 0; i < KEXEC_RSVD; i++) {
        pte = pte_setpaddr(pte, (ptr_t)&kpt_pa->kernel_lfts[i]);
        set_pte(klptep, pte);

        klptep++;
    }

    // Ensure the size of kernel is within the reservation
    pfn_t kimg_pagecount = 
        pfn(__far(__kexec_end) - __far(__kexec_start));
    if (kimg_pagecount > KEXEC_RSVD * _PAGE_LEVEL_SIZE) {
        // ERROR: require more pages
        //  here should do something else other than head into blocking
        asm("ud2");
    }

    // Now, map the kernel

    pfn_t kimg_end = pfn(to_kphysical(__far(__kexec_end)));
    pfn_t i = pfn(to_kphysical(__far(__kexec_text_start)));
    ktep += i;

    // kernel .text
    pte = pte_setprot(pte, KERNEL_EXEC);
    pfn_t ktext_end = pfn(to_kphysical(__far(__kexec_text_end)));
    for (; i < ktext_end; i++) {
        pte = pte_setpaddr(pte, page_addr(i));
        set_pte(ktep, pte);

        ktep++;
    }

    // all remaining kernel sections
    pte = pte_setprot(pte, KERNEL_DATA);
    for (; i < kimg_end; i++) {
        pte = pte_setpaddr(pte, page_addr(i));
        set_pte(ktep, pte);

        ktep++;
    }

    // XXX: Mapping the kernel .rodata section?

    // set mount point
    pte_t* kmntep = (pte_t*) &kpt_pa->l0t[pfn_at(PG_MOUNT_1, L0T_SIZE)];
    set_pte(kmntep, mkpte((ptr_t)kpt_pa->pg_mnt, KERNEL_DATA));

    // Build up self-reference
    int level = (VMS_SELF / L0T_SIZE) & _PAGE_LEVEL_MASK;
    
    pte = mkpte_root((ptr_t)kpt_pa, KERNEL_DATA);
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