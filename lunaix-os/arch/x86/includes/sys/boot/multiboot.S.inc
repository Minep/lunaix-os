#define __ASM__ 

#include "multiboot.h"

.section .multiboot
__mb_start:
    .4byte MULTIBOOT_MAGIC

#ifdef CONFIG_X86_BL_MB
    #define MB_FLAGS    (MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN)
    .4byte MB_FLAGS
    .4byte -(MULTIBOOT_MAGIC + MB_FLAGS)

#elif  CONFIG_X86_BL_MB2
    #define HDR_LEN     (__mb_end - __mb_start)
    
    .4byte   0
    .4byte   HDR_LEN
    .4byte   -(MULTIBOOT_MAGIC + HDR_LEN)

    .align  MULTIBOOT_TAG_ALIGN
    __mbir_tag_start:
    .2byte  MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    .2byte  0
    .4byte   __mbir_tag_end - __mbir_tag_start
    .4byte   MULTIBOOT_TAG_TYPE_CMDLINE
    .4byte   MULTIBOOT_TAG_TYPE_MMAP
    .4byte   MULTIBOOT_TAG_TYPE_BASIC_MEMINFO
    .4byte   MULTIBOOT_TAG_TYPE_MODULE
    __mbir_tag_end:

    .align  MULTIBOOT_TAG_ALIGN
    .2byte  MULTIBOOT_HEADER_TAG_END
    .2byte  0
    .4byte   8

#endif

__mb_end: