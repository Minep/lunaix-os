#ifndef __LUNAIX_AA64_MMU_H
#define __LUNAIX_AA64_MMU_H

#if    defined(CONFIG_AA64_PAGE_GRAN_4K)
#define _MMU_TG    0b01
#elif  defined(CONFIG_AA64_PAGE_GRAN_16K)
#define _MMU_TG    0b10
#elif  defined(CONFIG_AA64_PAGE_GRAN_64K)
#define _MMU_TG    0b11
#endif

#if CONFIG_AA_OA_SIZE == 52
#define _MMU_USE_OA52
#endif

#endif /* __LUNAIX_AA64_MMU_H */
