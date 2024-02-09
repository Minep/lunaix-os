#define __BOOT_CODE__

#include <lunaix/mm/pagetable.h>
#include <lunaix/compiler.h>

#include <sys/boot/bstage.h>
#include <sys/mm/mm_defs.h>

// Provided by linker (see linker.ld)
extern u8_t __kexec_start[];
extern u8_t __kexec_end[];
extern u8_t __kexec_text_start[];
extern u8_t __kexec_text_end[];
extern u8_t __kboot_start[];
extern u8_t __kboot_end[];

// define the initial page table layout
struct kernel_map {
    pte_t l0t[_PAGE_LEVEL_SIZE];
    pte_t lft_bootmap[_PAGE_LEVEL_SIZE];

    struct {
        pte_t _lft[_PAGE_LEVEL_SIZE];
    } kernel_lfts[16];
} align(4);

static struct kernel_map kernel_pt __section(".kpg");
export_symbol(debug, boot, kernel_pt);


void boot_text
_init_page()
{
    struct kernel_map* kpt_pa = (struct kernel_map*)to_kphysical(&kernel_pt);

    pte_t* kl0tep       = (pte_t*) &kpt_pa->l0t[pfn_at(KERNEL_RESIDENT, L0T_SIZE)];
    pte_t* kl1tep       = (pte_t*)  kpt_pa->kernel_lfts;    
    pte_t* boot_l0tep   = (pte_t*)  kpt_pa;
    pte_t* boot_l1tep   = (pte_t*) &kpt_pa->lft_bootmap;
    pte_t pte           = mkpte((ptr_t)boot_l1tep, KERNEL_DATA);

    pte_write_entry(boot_l0tep, pte);

    // Identity map on everything <= __kboot_end (which include first 1MiB)
    pfn_t kboot_end = pfn((ptr_t)__kboot_end);
    
    pte = pte_mkleaf(pte);
    for (size_t i = 0; i < kboot_end; i++) {
        pte = pte_setpaddr(pte, page_addr(i));
        pte_write_entry(boot_l1tep, pte);

        boot_l1tep++;
    }

    // --- 将内核重映射至高半区 ---

    // Hook the kernel reserved LFTs onto L0T
    pte = pte_mkroot(pte);
    for (u32_t i = 0; i < KEXEC_RSVD; i++) {
        pte = pte_setpaddr(pte, (ptr_t)&kpt_pa->kernel_lfts[i]);
        pte_write_entry(kl0tep, pte);

        kl0tep++;
    }

    // Ensure the size of kernel is within the reservation
    pfn_t kimg_pagecount = 
        pfn((ptr_t)__kexec_end - (ptr_t)__kexec_start);
    if (kimg_pagecount > KEXEC_RSVD * _PAGE_LEVEL_SIZE) {
        // ERROR: require more pages
        //  here should do something else other than head into blocking
        asm("ud2");
    }

    // Now, map the kernel

    pte = pte_mkleaf(pte);

    pfn_t kimg_end = pfn(to_kphysical(__kexec_end));
    pfn_t i = pfn(to_kphysical(__kexec_text_start));
    kl1tep += i;

    // kernel .text
    pte = pte_setprot(pte, KERNEL_EXEC);
    pfn_t ktext_end = pfn(to_kphysical(__kexec_text_end));
    for (; i < ktext_end; i++) {
        pte = pte_setpaddr(pte, page_addr(i));
        pte_write_entry(kl1tep, pte);

        kl1tep++;
    }

    // all remaining kernel sections
    pte = pte_setprot(pte, KERNEL_DATA);
    for (; i < kimg_end; i++) {
        pte = pte_setpaddr(pte, page_addr(i));
        pte_write_entry(kl1tep, pte);

        kl1tep++;
    }

    // XXX: Mapping the kernel .rodata section?

    // Build up self-reference
    pte = mkpte((ptr_t)kpt_pa, KERNEL_DATA);
    pte_write_entry(boot_l0tep + _PAGE_LEVEL_MASK, pte_mkroot(pte));
}

ptr_t boot_text
kpg_init()
{
    ptr_t kmap_pa = to_kphysical(&kernel_pt);
    for (size_t i = 0; i < sizeof(kernel_pt); i++) {
        ((u8_t*)kmap_pa)[i] = 0;
    }

    _init_page();

    return kmap_pa;
}