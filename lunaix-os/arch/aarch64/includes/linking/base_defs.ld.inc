#ifndef __LUNAIX_BASE_DEFS_LD_INC
#define __LUNAIX_BASE_DEFS_LD_INC

#define __LD__
#include <sys/mm/mempart.h>

#define KEXEC_BASE      KERNEL_IMG
#define PAGE_GRAN       4K

#define ENTRY_POINT     start_

#define LOAD_OFF        0x100000


#endif /* __LUNAIX_BASE_DEFS_LD_INC */
