#define __BOOT_CODE__

#include <lunaix/mm/pagetable.h>
#include <lunaix/compiler.h>

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>


ptr_t boot_text
kpg_init()
{
    return remap_kernel();
}