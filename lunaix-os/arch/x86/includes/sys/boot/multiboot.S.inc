#define __ASM__ 

#include <sys/boot/multiboot.h>

#define MB_FLAGS    (MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN)

.section .multiboot
    .long MULTIBOOT_MAGIC
    .long MB_FLAGS
    .long CHECKSUM(MB_FLAGS)
