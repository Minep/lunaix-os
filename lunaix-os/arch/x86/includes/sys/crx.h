#ifndef __LUNAIX_CRX_H
#define __LUNAIX_CRX_H

#define CR4_PSE36           ( 1UL << 4  )
#define CR4_OSXMMEXCPT      ( 1UL << 10 )
#define CR4_OSFXSR          ( 1UL << 9  )
#define CR4_PCIDE           ( 1UL << 17 )
#define CR4_PGE             ( 1UL << 7  )
#define CR4_LA57            ( 1UL << 12 )

#define CR0_PG              ( 1UL << 31 )
#define CR0_WP              ( 1UL << 16 )
#define CR0_EM              ( 1UL << 2  )
#define CR0_MP              ( 1UL << 1  )

#define crx_addflag(crx, flag)      \
    asm(                            \
        "movl %%" #crx ", %%eax\n"  \
        "orl  %0,    %%eax\n"       \
        "movl %%eax, %%" #crx "\n"  \
        ::"r"(flag)                 \
        :"eax"                      \
    );

#define crx_rmflag(crx, flag)       \
    asm(                            \
        "movl %%" #crx ", %%eax\n"  \
        "andl  %0,    %%eax\n"      \
        "movl %%eax, %%" #crx "\n"  \
        ::"r"(~(flag))              \
        :"eax"                      \
    );

static inline void
cr4_setfeature(unsigned long feature)
{
    crx_addflag(cr4, feature);
}

static inline void
cr0_setfeature(unsigned long feature)
{
    crx_addflag(cr0, feature);
}

static inline void
cr0_unsetfeature(unsigned long feature)
{
    crx_rmflag(cr0, feature);
}

#endif /* __LUNAIX_CR4_H */
