#ifndef __LUNAIX_ARCH_BOOT_STAGE_H
#define __LUNAIX_ARCH_BOOT_STAGE_H
#include <lunaix/types.h>
#include <lunaix/boot_generic.h>

extern ptr_t __multiboot_addr;

extern u8_t __kboot_start[];
extern u8_t __kboot_end[];

#include <asm-generic/boot_stage.h>

ptr_t 
remap_kernel();

#endif /* __LUNAIX_ARCH_BOOT_STAGE_H */
