#define __ASM__ 

#include <linking/base_defs.ld.inc>
#include "multiboot.h"

.section .multiboot
__mb_start:
    .4byte MULTIBOOT_MAGIC

/*
    One may wondering why we set the address header part.

    This is due to some weirdo's patch to QEMU that prohibit loading
    ELF64 using -kernel option. The only way to get around is to
    fool multiboot loader by pretend ourselves a non-elf kernel.

    Although one may think this "trick" is some-how not portable,
    the fact is actually the other-way around. It allow us to relax
    the assumption that grub (or other multiboot compliances) made on
    our executable.
 */

#ifdef CONFIG_X86_BL_MB
    #define MB_FLAGS    ( MULTIBOOT_MEMORY_INFO |\
                          MULTIBOOT_PAGE_ALIGN  |\
                          MULTIBOOT_AOUT_KLUDGE    )
    .4byte MB_FLAGS
    .4byte -(MULTIBOOT_MAGIC + MB_FLAGS)
    .4byte __mb_start
    .4byte __kload_start
    .4byte __kload_end
    .4byte __kbss_end
    .4byte ENTRY_POINT

#elif  CONFIG_X86_BL_MB2
    #define HDR_LEN     (__mb_end - __mb_start)
    
    .4byte   0
    .4byte   HDR_LEN
    .4byte   -(MULTIBOOT_MAGIC + HDR_LEN)

    .align  MULTIBOOT_TAG_ALIGN
    __mbir_tag_start:
    .2byte   MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    .2byte   0
    .4byte   __mbir_tag_end - __mbir_tag_start
    .4byte   MULTIBOOT_TAG_TYPE_CMDLINE
    .4byte   MULTIBOOT_TAG_TYPE_MMAP
    .4byte   MULTIBOOT_TAG_TYPE_BASIC_MEMINFO
    .4byte   MULTIBOOT_TAG_TYPE_MODULE
    __mbir_tag_end:

    __maddr_tag_start:
    .2byte   MULTIBOOT_HEADER_TAG_ADDRESS
    .2byte   0
    .4byte   __maddr_tag_end - __maddr_tag_start
    .4byte   __mb_start
    .4byte   __kload_start
    .4byte   __kload_end
    .4byte   __kbss_end
    __maddr_tag_end:

    __ment_tag_start:
    .2byte   MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
    .2byte   0
    .4byte   __ment_tag_end - __ment_tag_start
    .4byte ENTRY_POINT
    __ment_tag_end:

    .align  MULTIBOOT_TAG_ALIGN
    .2byte  MULTIBOOT_HEADER_TAG_END
    .2byte  0
    .4byte  8

#endif

__mb_end: