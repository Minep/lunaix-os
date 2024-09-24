#define __BOOT_CODE__

#include <lunaix/boot_generic.h>

#include <sys/boot/bstage.h>
#include <sys/boot/multiboot.h>
#include <sys-generic/bootmem.h>

#include <asm/mempart.h>

#include <klibc/string.h>

#define MEM_1M      0x100000UL

static void
mb_parse_cmdline(struct boot_handoff* bhctx, char* cmdline)
{
    size_t slen;
    char* cmd;
    
    slen = strlen(cmdline);
    if (!slen) {
        return;
    }

    cmd = bootmem_alloc(slen + 1);
    strncpy(cmd, cmdline, slen);

    bhctx->kexec.len = slen;
    bhctx->kexec.cmdline = cmd;
}

static void
mb_parse_mmap(struct boot_handoff* bhctx,
              struct multiboot_info* mb)
{
    struct multiboot_mmap_entry *mb_mmap, *mb_mapent;
    size_t mmap_len;
    struct boot_mmapent *bmmap, *bmmapent;

    mb_mmap = (struct multiboot_mmap_entry*)__ptr(mb->mmap_addr);
    mmap_len = mb->mmap_length / sizeof(*mb_mmap);

    bmmap = bootmem_alloc(sizeof(*bmmap) * mmap_len);
    
    for (size_t i = 0; i < mmap_len; i++) {
        mb_mapent = &mb_mmap[i];
        bmmapent  = &bmmap[i];
    
        if (mb_mapent->type == MULTIBOOT_MEMORY_AVAILABLE) 
        {
            bmmapent->type = BOOT_MMAP_FREE;
        } 

        else if (mb_mapent->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) 
        {
            bmmapent->type = BOOT_MMAP_RCLM;
        } 

        else {
            bmmapent->type = BOOT_MMAP_RSVD;
        }

        bmmapent->start = mb_mapent->addr_low;
        bmmapent->size = mb_mapent->len_low;
    }

    bhctx->mem.size = (mb->mem_upper << 10) + MEM_1M;
    bhctx->mem.mmap = bmmap;
    bhctx->mem.mmap_len = mmap_len;
}

static void
mb_parse_mods(struct boot_handoff* bhctx,
              struct multiboot_info* mb)
{
    if (!mb->mods_count) {
        bhctx->mods.mods_num = 0;
        return;
    }

    struct boot_modent* modents;
    struct multiboot_mod_list* mods, *mod;
    size_t name_len;
    char* mod_name, *cmd;

    mods = (struct multiboot_mod_list*)__ptr(mb->mods_addr);
    modents = bootmem_alloc(sizeof(*modents) * mb->mods_count);

    for (size_t i = 0; i < mb->mods_count; i++) {
        mod = &mods[i];
        cmd = (char*)__ptr(mod->cmdline);
        name_len = strlen(cmd);
        mod_name = bootmem_alloc(name_len + 1);

        modents[i] = (struct boot_modent){ 
            .start = mod->mod_start,
            .end = mod->mod_end,
            .str = mod_name 
        };

        strncpy(mod_name, cmd, name_len);
    }

    bhctx->mods.mods_num = mb->mods_count;
    bhctx->mods.entries = modents;
}

static void
mb_prepare_hook(struct boot_handoff* bhctx)
{
    // nothing to do
}

static void
mb_release_hook(struct boot_handoff* bhctx)
{
    // nothing to do
}

void
mb_parse(struct boot_handoff* bhctx)
{
    struct multiboot_info* mb;
    
    mb = (struct multiboot_info*)__multiboot_addr;

    /* Parse memory map */
    if ((mb->flags & MULTIBOOT_INFO_MEM_MAP)) {
        mb_parse_mmap(bhctx, mb);
    }

    /* Parse cmdline */
    if ((mb->flags & MULTIBOOT_INFO_CMDLINE)) {
        mb_parse_cmdline(bhctx, (char*)__ptr(mb->cmdline));
    }

    /* Parse sys modules */
    if ((mb->flags & MULTIBOOT_INFO_MODS)) {
        mb_parse_mods(bhctx, mb);
    }

    bhctx->prepare = mb_prepare_hook;
    bhctx->release = mb_release_hook;
}