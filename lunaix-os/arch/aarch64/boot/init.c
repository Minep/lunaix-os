#include <lunaix/boot_generic.h>
#include <asm/aa64.h>

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
    set_sysreg(VBAR_EL1, aa64_vbase);
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

static inline void
extract_dtb_bootinfo(ptr_t dtb, struct boot_handoff* handoff)
{
    handoff->kexec.dtb_pa = dtb;
    
    // TODO extract /memory, /reserved-memories from dtb
}

struct boot_handoff*
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