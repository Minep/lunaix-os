#include <lunaix/boot_generic.h>
#include <asm/aa64.h>
#include <asm/aa64_spsr.h>
#include <hal/devtree.h>

#include "init.h"

static inline void
setup_pstate()
{
    /*
        SCTRL_EL1
            EE=0, E0E=0     // all little endian
            WXN=1           // write implie exec never
            SA0=1, SA=1     // alignment check on SP
            A=1             // alignment check on memref
            NMI=1           // mask interrupt
            M=1             // enable mmu
    */

   unsigned long sctrl = 0;

   sctrl |= SCTRL_NMI;
   sctrl |= SCTRL_WXN | SCTRL_nAA;
   sctrl |= SCTRL_SA | SCTRL_SA0;
   sctrl |= SCTRL_A | SCTRL_M;

   set_sysreg(TCR_EL1, sctrl);
   set_sysreg(SPSel, 1);
}

extern void aa64_vbase();

static inline void
setup_evbar()
{
    set_sysreg(VBAR_EL1, __ptr(aa64_vbase));
}

static inline void
setup_ttbr()
{
    /*
        
        TCR_EL1
            SH0=3           // Inner sharable
            ORGN0=0         // Normal memory, Outer Non-cacheable.
            IRGN0=1         // Normal memory, Inner Write-Back Read-Allocate Write-Allocate Cacheable.
            A1=0            // TTBR0 define ASID
            EPD1=0
            T1SZ=0
            EPD0=1
            T0SZ=16         // disable TTBR1, Use TTBR0 for all translation
            TG0=0           // VA48, 256T, 4K Granule
            TBI1=0,
            TBI0=0          // Ignore top bits
            AS=1            // 16bits asid
            HA=1
            HD=1            // Hardware managed dirty and access


        We may use the follow practice later
            TTBR0:  Translation for user-land (lowmem)
            TTBR1:  Translation for kernel-land (highmem)
    */

    unsigned long tcr = 0;
    ptr_t ttb;

    tcr |= TCR_T1SZ(0) | TCR_T0SZ(16);
    tcr |= TCR_TG0(TCR_G4K);
    tcr |= TCR_AS | TCR_HA | TCR_HD;
    tcr |= TCR_EPD0;

    ttb = kremap();

    set_sysreg(TTBR0_EL1, ttb);
    set_sysreg(TCR_EL1, tcr);
}

#define MMAP_ENTS_CHUNK_SIZE    16

static inline void
extract_dtb_bootinfo(ptr_t dtb, struct boot_handoff* handoff)
{
    struct fdt_blob fdt;
    struct fdt_memscan mscan;
    struct dt_memory_node mnode;

    int mmap_len = 0, mmap_max_len = 16;
    size_t pmem_size = 0;
    struct boot_mmapent* mmap;

    mmap = bootmem_alloc(sizeof(*mmap) * MMAP_ENTS_CHUNK_SIZE);
    handoff->kexec.dtb_pa = dtb;
    
    // TODO extract /memory, /reserved-memories from dtb

    fdt_load(&fdt, dtb);
    fdt_memscan_begin(&mscan, &fdt);

    struct boot_mmapent* mmap_ent;

    while(fdt_memscan_nextnode(&mscan, &fdt))
    {
        while (fdt_memscan_nextrange(&mscan, &mnode)) 
        {
            mmap_ent = &mmap[mmap_len++];
            *mmap_ent = (struct boot_mmapent) {
                .size = mnode.size,
                .start = mnode.base
            };

            if (mnode.type == FDT_MEM_FREE) {
                mmap_ent->type = BOOT_MMAP_FREE;
                pmem_size += mnode.size;
            }
            else {
                mmap_ent->type = BOOT_MMAP_RSVD;
            }


            if (mmap_len < mmap_max_len) {
                continue;
            }

            mmap_max_len += MMAP_ENTS_CHUNK_SIZE;

            // another allocation is just expanding the previous allocation.
            bootmem_alloc(sizeof(*mmap) * MMAP_ENTS_CHUNK_SIZE);
        }
    }

    handoff->mem.mmap = mmap;
    handoff->mem.mmap_len = mmap_len;
    handoff->mem.size = pmem_size;
}

static inline void
setup_gic_sysreg()
{
    int el;
    bool has_el2;
    u64_t pfr;

    pfr = read_sysreg(ID_AA64PFR0_EL1);
    el = read_sysreg(CurrentEL) >> 2;

    // Arm A-Profile: D19.2.69, check for GIC sysreg avaliability

    if (!BITS_GET(pfr, BITFIELD(27, 24))) {
        return;
    }

    has_el2 = !!BITS_GET(pfr, BITFIELD(11, 8));

    // GIC spec (v3,v4): 12.1.7, table 12-2
    // GIC spec (v3,v4): ICC_SRE_ELx accessing
    // Arm A-Profile: R_PCDTX, PSTATE.EL reset value

    if (el == 3) {
        sysreg_flagging(ICC_SRE_EL3, ICC_SRE_SRE, 0);
    }
    
    if (has_el2) {
        // el must be >= 2
        sysreg_flagging(ICC_SRE_EL2, ICC_SRE_SRE, 0);
    }

    sysreg_flagging(ICC_SRE_EL1, ICC_SRE_SRE, 0);
}

void boot_text
aarch64_pre_el1_init()
{
    setup_gic_sysreg();
}

bool boot_text
aarch64_prepare_el1_transfer()
{
    ptr_t spsr;
    int el;

    el = read_sysreg(CurrentEL) >> 2;

    if (el == 1) {
        // no transfer required
        return false;
    }

    spsr  = SPSR_EL1_preset;
    spsr |= SPSR_AllInt | SPSR_I | SPSR_F | SPSR_A;

    if (el == 2) {
        set_sysreg(SPSR_EL2, spsr);
    }
    else {
        set_sysreg(SPSR_EL3, spsr);
    }

    return true;
}

struct boot_handoff* boot_text
aarch64_init(ptr_t dtb)
{
    setup_evbar();
    setup_ttbr();
    setup_pstate();

    struct boot_handoff* handoff;
    
    handoff = bootmem_alloc(sizeof(*handoff));

    extract_dtb_bootinfo(dtb, handoff);

    return handoff;
}